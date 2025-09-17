#define V1_ABSCENCE_VERDICT

#include "view.hpp"

#include <queue>

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/engine/task/task.hpp>
#include <userver/engine/task/task_with_result.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/async.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "definitions/all.hpp"

using json = nlohmann::json;

namespace views::v1::abscence::verdict {

namespace {

std::optional<std::string> ActionTypeToName(const std::string& type) {
  if (type == "vacation") {
    return "отпуск";
  } else if (type == "sick_leave") {
    return "больничный";
  } else if (type == "business_trip") {
    return "командировку";
  } else if (type == "unpaid_vacation") {
    return "неоплачиваемый отпуск";
  } else if (type == "overtime") {
    return "сверхурочное время";
  } else if (type == "vacation_days_instead") {
    return "дни в счёт ежегодного отпуска";
  } else if (type == "payout_birth") {
    return "единовременную выплату при рождении ребёнка";
  } else if (type == "certificates_on_dismissal") {
    return "выдачу справок при увольнении";
  } else if (type == "tax_deduction_children") {
    return "налоговый вычет на детей";
  } else if (type == "part_time") {
    return "неполный рабочий день";
  } else if (type == "maternity_childcare_15") {
    return "отпуск по уходу за ребёнком до 1.5 лет";
  } else if (type == "maternity_childcare_3") {
    return "отпуск по уходу за ребёнком до 3 лет";
  } else if (type == "maternity_pregnancy") {
    return "отпуск по беременности и родам";
  } else if (type == "transfer") {
    return "перевод на другую должность";
  } else if (type == "vacation_shift") {
    return "перенос ежегодного отпуска";
  } else if (type == "maternity_work_during") {
    return "работу в период декретного отпуска";
  } else if (type == "personal_data_change") {
    return "смену персональных данных";
  } else if (type == "resignation") {
    return "увольнение по собственному желанию";
  } else if (type == "maternity_early_exit") {
    return "досрочный выход из отпуска по уходу";
  } else if (type == "unpaid_vacation_with_reason") {
    return "отпуск без сохранения ЗП (с указанием причины)";
  }
  return std::nullopt;
}

struct ActionInfo {
  std::string employee_id, type;
  std::optional<std::string> document_id;
  userver::storages::postgres::TimePoint start_date, end_date;
};

struct EmployeeInfo {
  std::string name, surname, subcompany;
  std::optional<std::string> patronymic, head_id, job_position;
};

struct HeadInfo {
  std::string name, surname;
  std::optional<std::string> patronymic, job_position;
};

struct VacationDocumentRequest {
  std::string action_id, user_id, company_id;
};

void GenerateVacationDocument(
    VacationDocumentRequest&& request,
    userver::storages::postgres::ClusterPtr pg_cluster,
    userver::clients::http::Client& http_client,
    const std::string& pyservice_url) {
  auto action_id = request.action_id;
  auto request_type = "create";
  auto user_id = request.user_id;
  const auto& company_id = request.company_id;

  auto trx =
      pg_cluster->Begin("documents_vacation",
                        userver::storages::postgres::ClusterHostType::kMaster,
                        {});  // TODO: change to slave read tx

  auto action_info =
      trx.Execute(
             "SELECT user_id, type, document_id, start_date, end_date "
             "FROM working_day_" +
                 company_id +
                 ".actions "
                 "WHERE id = $1",
             action_id)
          .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);

  auto employee_info =
      trx.Execute(
             "SELECT name, surname, subcompany, patronymic, head_id, job_position "
             "FROM working_day_" +
                 company_id +
                 ".employees "
                 "WHERE id = $1 ",
             action_info.employee_id)
          .AsSingleRow<EmployeeInfo>(userver::storages::postgres::kRowTag);

  auto head_info =
      trx.Execute(
             "SELECT name, surname, patronymic, job_position "
             "FROM working_day_" +
                 company_id +
                 ".employees "
                 "WHERE id = $1 ",
             employee_info.head_id.value_or(action_info.employee_id))
          .AsSingleRow<HeadInfo>(userver::storages::postgres::kRowTag);

  trx.Commit();

  PyserviceDocumentGenerateRequest link_request;
  link_request.action_type = action_info.type;
  link_request.request_type = request_type;
  link_request.employee_id = action_info.employee_id;
  link_request.employee_name = employee_info.name;
  link_request.employee_surname = employee_info.surname;
  link_request.subcompany = employee_info.subcompany;
  link_request.company_id = company_id;
  link_request.head_name = head_info.name;
  link_request.head_surname = head_info.surname;
  link_request.start_date = userver::utils::datetime::Timestring(
      action_info.start_date, "UTC", "%d.%m.%Y");
  link_request.end_date = userver::utils::datetime::Timestring(
      action_info.end_date, "UTC", "%d.%m.%Y"),
  link_request.employee_patronymic = employee_info.patronymic;
  link_request.head_patronymic = head_info.patronymic;
  link_request.employee_position = employee_info.job_position;
  link_request.head_position = head_info.job_position;

  auto file_key = action_info.document_id.value_or(".pdf");
  file_key = file_key.substr(0, file_key.size() - 4);

  auto response = http_client.CreateRequest()
                      .post(pyservice_url + "?file_key=" + file_key)
                      .data(link_request.ToJsonString())
                      .retry(2)  // retry once in case of error
                      .timeout(std::chrono::milliseconds{10000})
                      .perform();  // start performing the request
  response->raise_for_status();

  auto action_name = ActionTypeToName(action_info.type);

  auto document_name = "Запрос на " + action_name.value() + " " +
                       employee_info.surname + " " + employee_info.name + " " +
                       userver::utils::datetime::Timestring(
                           action_info.start_date, "UTC", "%d.%m.%Y") +
                       " - " +
                       userver::utils::datetime::Timestring(
                           action_info.end_date, "UTC", "%d.%m.%Y");

  auto file_key_signed = file_key + "_signed.pdf";
  file_key += ".pdf";

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "INSERT INTO working_day_" + company_id +
                          ".documents(id, name, "
                          "sign_required, type) "
                          "VALUES($1, $2, $3, $4)",
                      file_key_signed, document_name, true, "employee_request");

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "INSERT INTO working_day_" + company_id +
                          ".employee_document "
                          "(employee_id, document_id, signed) "
                          "VALUES ($1, $2, $3), ($4, $2, $3) "
                          "ON CONFLICT DO NOTHING",
                      action_info.employee_id, file_key_signed, true,
                      employee_info.head_id.value_or(action_info.employee_id));

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "UPDATE working_day_" + company_id + ".documents "
                          "SET sign_required = TRUE "
                          "WHERE id = $1",
                       action_info.document_id);

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "UPDATE working_day_" + company_id + ".documents "
                          "SET sign_required = TRUE "
                          "WHERE id = $1",
                       action_info.document_id);

  auto trx =
      pg_cluster->Begin("documents_vacation",
                        userver::storages::postgres::ClusterHostType::kMaster,
                        {});  // TODO: change to slave read tx

  auto action_info =
      trx.Execute(
             "SELECT user_id, type, document_id, start_date, end_date "
             "FROM working_day_" +
                 company_id +
                 ".actions "
                 "WHERE id = $1",
             action_id)
          .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);

  auto employee_info =
      trx.Execute(
             "SELECT name, surname, subcompany, patronymic, head_id, job_position "
             "FROM working_day_" +
                 company_id +
                 ".employees "
                 "WHERE id = $1 ",
             action_info.employee_id)
          .AsSingleRow<EmployeeInfo>(userver::storages::postgres::kRowTag);

  auto head_info =
      trx.Execute(
             "SELECT name, surname, patronymic, job_position "
             "FROM working_day_" +
                 company_id +
                 ".employees "
                 "WHERE id = $1 ",
             employee_info.head_id.value_or(action_info.employee_id))
          .AsSingleRow<HeadInfo>(userver::storages::postgres::kRowTag);

  trx.Commit();

  PyserviceDocumentGenerateRequest link_request;
  link_request.action_type = action_info.type;
  link_request.request_type = request_type;
  link_request.employee_id = action_info.employee_id;
  link_request.employee_name = employee_info.name;
  link_request.employee_surname = employee_info.surname;
  link_request.subcompany = employee_info.subcompany;
  link_request.company_id = company_id;
  link_request.head_name = head_info.name;
  link_request.head_surname = head_info.surname;
  link_request.start_date = userver::utils::datetime::Timestring(
      action_info.start_date, "UTC", "%d.%m.%Y");
  link_request.end_date = userver::utils::datetime::Timestring(
      action_info.end_date, "UTC", "%d.%m.%Y"),
  link_request.employee_patronymic = employee_info.patronymic;
  link_request.head_patronymic = head_info.patronymic;
  link_request.employee_position = employee_info.job_position;
  link_request.head_position = head_info.job_position;

  auto file_key = action_info.document_id.value_or(".pdf");
  file_key = file_key.substr(0, file_key.size() - 4);

  auto response = http_client.CreateRequest()
                      .post(pyservice_url + "?file_key=" + file_key)
                      .data(link_request.ToJsonString())
                      .retry(2)  // retry once in case of error
                      .timeout(std::chrono::milliseconds{10000})
                      .perform();  // start performing the request
  response->raise_for_status();

  auto action_name = ActionTypeToName(action_info.type);

  auto document_name = "Запрос на " + action_name.value() + " " +
                       employee_info.surname + " " + employee_info.name + " " +
                       userver::utils::datetime::Timestring(
                           action_info.start_date, "UTC", "%d.%m.%Y") +
                       " - " +
                       userver::utils::datetime::Timestring(
                           action_info.end_date, "UTC", "%d.%m.%Y");

  auto file_key_signed = file_key + "_signed.pdf";
  file_key += ".pdf";

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "INSERT INTO working_day_" + company_id +
                          ".documents(id, name, "
                          "sign_required, type) "
                          "VALUES($1, $2, $3, $4)",
                      file_key_signed, document_name, true, "employee_request");

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "DELETE FROM working_day_" + company_id + ".employee_document "
                      "WHERE employee_id = $1 AND document_id = $2",
                      action_info.employee_id, file_key);

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "UPDATE working_day_" + company_id + ".documents "
                      "SET parent_id = $2 "
                      "WHERE id = $1",
                      file_key, file_key_signed);

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "INSERT INTO working_day_" + company_id +
                          ".employee_document "
                          "(employee_id, document_id, signed) "
                          "VALUES ($1, $2, $3), ($4, $2, $3) "
                          "ON CONFLICT DO NOTHING",
                      action_info.employee_id, file_key_signed, true,
                      employee_info.head_id.value_or(action_info.employee_id));

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "UPDATE working_day_" + company_id + ".documents "
                          "SET sign_required = TRUE "
                          "WHERE id = $1",
                       action_info.document_id);

}

class AbscenceVerdictHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-abscence-verdict";

  AbscenceVerdictHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()),
        http_client_(
            component_context.FindComponent<userver::components::HttpClient>()
                .GetHttpClient()),
        pyservice_url(config["pyservice-url"].As<std::string>()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    AbscenceVerdictRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());
    auto user_id = ctx.GetData<std::string>("user_id");
    auto company_id = ctx.GetData<std::string>("company_id");

    auto trx = pg_cluster_->Begin(
        "verdict_abscence",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    auto action_info =
        trx.Execute(
               "SELECT user_id, type, document_id, start_date, end_date "
               "FROM working_day_" +
                   company_id +
                   ".actions "
                   "WHERE id = $1",
               request_body.action_id)
            .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);

    if (!action_info.document_id) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Document doesn't exist"}.ToJsonString();
    }

    auto action_name = ActionTypeToName(action_info.type);
    std::string notification_text =
        "Ваш запрос на " + action_name.value() + " с " +
        userver::utils::datetime::Timestring(action_info.start_date, "UTC",
                                             "%d.%m.%Y") +
        " по " +
        userver::utils::datetime::Timestring(action_info.end_date, "UTC",
                                             "%d.%m.%Y");
    std::string action_status = "denied";
    if (request_body.approve) {
      action_status = "approved";
      notification_text += " был одобрен.";
    } else {
      notification_text += " был отклонен.";
    }


    if (request_body.notification_id.has_value()) {
      auto result = trx.Execute("DELETE FROM working_day_" + company_id +
                                    ".notifications "
                                    "WHERE id = $1 ",
                                request_body.notification_id.value());
    }

    auto notification_id = userver::utils::generators::GenerateUuid();
    std::optional<std::string> maybe_action_id = request_body.approve ? std::optional(request_body.action_id) : std::nullopt;
    auto result =
        trx.Execute("INSERT INTO working_day_" + company_id +
                        ".notifications(id, type, text, user_id, "
                        "sender_id, action_id) "
                        "VALUES($1, $2, $3, $4, $5, $6) "
                        "ON CONFLICT (id) "
                        "DO NOTHING",
                    notification_id, action_info.type + "_" + action_status,
                    notification_text, action_info.employee_id, user_id,
                    maybe_action_id);

    if (request_body.approve) {
      auto result = trx.Execute("UPDATE working_day_" + company_id +
                                    ".actions "
                                    "SET status = $2 "
                                    "WHERE id = $1 ",
                                request_body.action_id, action_status);
    } else {
      auto result = trx.Execute("DELETE FROM working_day_" + company_id +
                                    ".actions "
                                    "WHERE id = $1 ",
                                request_body.action_id);
    }

    trx.Commit();

    if (request_body.approve) {
      auto tasks = tasks_.Lock();
      while (!tasks->empty() && tasks->front().IsFinished()) {
        tasks->pop();
      }

      VacationDocumentRequest request{.action_id = request_body.action_id,
                                      .user_id = user_id,
                                      .company_id = company_id};

      tasks->push(userver::utils::AsyncBackground(
          "GenerateVacationDocument",
          userver::engine::current_task::GetTaskProcessor(),
          [req = std::move(request), this]() mutable -> int {
            GenerateVacationDocument(std::move(req), this->pg_cluster_,
                                     this->http_client_, this->pyservice_url);
            return 42;
          }));
    }

    return "";
  }

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Abscence verdict handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  mutable userver::concurrent::Variable<
      std::queue<userver::engine::TaskWithResult<int>>>
      tasks_;
  std::string pyservice_url;
};

}  // namespace

void AppendAbscenceVerdict(userver::components::ComponentList& component_list) {
  component_list.Append<AbscenceVerdictHandler>();
}

}  // namespace views::v1::abscence::verdict