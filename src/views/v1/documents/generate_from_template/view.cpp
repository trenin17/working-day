#define V1_DOCUMENTS_GENERATE_FROM_TEMPLATE

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/http/url.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>


#include "definitions/all.hpp"

using json = nlohmann::json;

namespace views::v1::documents::generate_from_template {

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
class HeadTemplate {
 public:
  std::string head_template;
};

class DocumentsGenerateFromTemplateHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-generate-from-template";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Documents generate from template handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

  DocumentsGenerateFromTemplateHandler(
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
        pyservice_url_(config["pyservice-url"].As<std::string>()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    auto user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto action_id = request.GetArg("action_id");
    LOG_INFO() << "action_id: " << action_id;
    GenerateFromTemplateRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto file_key = userver::utils::generators::GenerateUuid();

    auto trx = pg_cluster_->Begin(
        "documents_generate_from_template",
        userver::storages::postgres::ClusterHostType::kMaster,
        {});  // TODO: change to slave read tx


    auto action_info =
        trx.Execute(
               "SELECT user_id, type, start_date, end_date "
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

    auto head_template = trx.Execute(
               "SELECT head_template "
               "FROM wd_general.companies "
                   "WHERE id = $1 ",
                company_id)
                .AsSingleRow<HeadTemplate>(userver::storages::postgres::kRowTag).head_template;

    trx.Commit();

    PyserviceDocumentGenerateRequest link_request;
    link_request.action_type = action_info.type;
    link_request.request_type = "create";
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
    if (request_body.params.has_value()) {
      link_request.params = request_body.params.value();
    }
    link_request.head_template = head_template;


    auto resp = http_client_.CreateRequest()
                    .post(
                        "http://python-service:3000/"
                        "document/generate-from-template?file_key=" +
                        file_key
                    )
                    .data(link_request.ToJsonString())
                    .retry(2)
                    .timeout(std::chrono::milliseconds{5000})
                    .perform();

    resp->raise_for_status();
    GenerateFromTemplateResponse result;
    result.download_link = resp->body();


    auto action_name = ActionTypeToName(action_info.type);

    auto document_name = "Запрос на " + action_name.value() + " " + employee_info.surname + " " +
                         employee_info.name + " " +
                         userver::utils::datetime::Timestring(
                             action_info.start_date, "UTC", "%d.%m.%Y") +
                         " - " +
                         userver::utils::datetime::Timestring(
                             action_info.end_date, "UTC", "%d.%m.%Y");
    file_key += ".pdf";

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "INSERT INTO working_day_" + company_id +
                             ".documents(id, name, "
                             "sign_required, type, author_id) "
                             "VALUES($1, $2, $3, $4, $5)",
                         file_key, document_name, 0, "employee_request", user_id);

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "UPDATE working_day_" + company_id + ".actions "
                             "SET document_id = $1 "
                             "WHERE id = $2",
                         file_key, action_id);

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".employee_document "
            "(employee_id, document_id, signed) "
            "VALUES ($1, $2, $3), ($4, $2, $3) "
            "ON CONFLICT DO NOTHING",
        action_info.employee_id, file_key, false,
        employee_info.head_id.value_or(action_info.employee_id));

    return result.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsGenerateFromTemplate(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsGenerateFromTemplateHandler>();
}

}  // namespace views::v1::documents::generate_from_template
