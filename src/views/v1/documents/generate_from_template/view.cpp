#define V1_DOCUMENTS_GENERATE_FROM_TEMPLATE
#define USERVER_POSTGRES_ENABLE_LEGACY_TIMESTAMP 1

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

#include <fmt/core.h>

#include "definitions/all.hpp"
#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::documents::generate_from_template {

namespace {

const std::string kTz = "UTC";

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

std::optional<std::string> BuildHeadNotificationText(
    const std::string& type,
    const userver::storages::postgres::TimePoint& start_date,
    const userver::storages::postgres::TimePoint& end_date) {
  constexpr auto notification_fmt =
      "Ваш сотрудник запросил {} c {} по {}. Подтвердите или отклоните "
      "запрос.";
  auto start = userver::utils::datetime::Timestring(start_date, kTz, "%Y-%m-%d");
  auto end = userver::utils::datetime::Timestring(end_date, kTz, "%Y-%m-%d");
  auto start_time =
      userver::utils::datetime::Timestring(start_date, kTz, "%Y-%m-%d %H:%M:%S");
  auto end_time =
      userver::utils::datetime::Timestring(end_date, kTz, "%Y-%m-%d %H:%M:%S");

  if (type == "vacation") {
    return fmt::format(notification_fmt, "отпуск", start, end);
  } else if (type == "sick_leave") {
    return fmt::format(notification_fmt, "больничный", start, end);
  } else if (type == "unpaid_vacation") {
    return fmt::format(notification_fmt, "неоплачиваемый отпуск", start, end);
  } else if (type == "business_trip") {
    return fmt::format(notification_fmt, "командировку", start, end);
  } else if (type == "overtime") {
    return fmt::format(notification_fmt, "сверхурочные", start_time, end_time);
  } else if (type == "vacation_days_instead") {
    return fmt::format(notification_fmt, "дни в счёт ежегодного отпуска", start,
                     end);
  } else if (type == "payout_birth") {
    return fmt::format(notification_fmt,
                     "единовременную выплату при рождении ребёнка", start,
                     end);
  } else if (type == "certificates_on_dismissal") {
    return fmt::format(notification_fmt, "выдачу справок при увольнении", start,
                     end);
  } else if (type == "tax_deduction_children") {
    return fmt::format(notification_fmt, "налоговый вычет на детей", start, end);
  } else if (type == "part_time") {
    return fmt::format(notification_fmt, "неполный рабочий день", start, end);
  } else if (type == "maternity_childcare_15") {
    return fmt::format(notification_fmt, "отпуск по уходу за ребёнком до 1.5 лет",
                     start, end);
  } else if (type == "maternity_childcare_3") {
    return fmt::format(notification_fmt, "отпуск по уходу за ребёнком до 3 лет",
                     start, end);
  } else if (type == "maternity_pregnancy") {
    return fmt::format(notification_fmt, "отпуск по беременности и родам", start,
                     end);
  } else if (type == "transfer") {
    return fmt::format(notification_fmt, "перевод на другую должность", start,
                     end);
  } else if (type == "vacation_shift") {
    return fmt::format(notification_fmt, "перенос ежегодного отпуска", start,
                     end);
  } else if (type == "maternity_work_during") {
    return fmt::format(notification_fmt, "работу в период декретного отпуска",
                     start, end);
  } else if (type == "personal_data_change") {
    return fmt::format(notification_fmt, "смену персональных данных", start,
                     end);
  } else if (type == "resignation") {
    return fmt::format(notification_fmt, "увольнение по собственному желанию",
                     start, end);
  } else if (type == "maternity_early_exit") {
    return fmt::format(notification_fmt, "досрочный выход из отпуска по уходу",
                     start, end);
  } else if (type == "unpaid_vacation_with_reason") {
    return fmt::format(notification_fmt,
                     "отпуск без сохранения ЗП (с указанием причины)", start,
                     end);
  }
  return std::nullopt;
}

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
    is_testing:
        type: boolean
        description: Use stub S3 presigned URLs in testsuite
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
        pyservice_url_(config["pyservice-url"].As<std::string>()),
        is_testing_(config["is_testing"].As<bool>()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    auto user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    GenerateFromTemplateRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto notification_text = BuildHeadNotificationText(
        request_body.type, request_body.start_date, request_body.end_date);
    if (!notification_text.has_value()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Unknown abscence type"}.ToJsonString();
    }

    if (request_body.type != "overtime") {
      using namespace userver::utils::datetime;
      using namespace std::literals::chrono_literals;
      request_body.start_date = Stringtime(
          Timestring(request_body.start_date, kTz, "%Y-%m-%d"), kTz, "%Y-%m-%d");
      request_body.end_date =
          Stringtime(Timestring(request_body.end_date, kTz, "%Y-%m-%d"), kTz,
                     "%Y-%m-%d") +
          1439min;
    }

    auto action_id = userver::utils::generators::GenerateUuid();
    const std::string action_status = "pending";

    auto trx = pg_cluster_->Begin(
        "documents_generate_from_template_read",
        userver::storages::postgres::ClusterHostType::kMaster,
        {});

    auto employee_info =
        trx.Execute(
               "SELECT name, surname, subcompany, patronymic, head_id, job_position "
               "FROM working_day_" +
                   company_id +
                   ".employees "
                   "WHERE id = $1 ",
               user_id)
            .AsSingleRow<EmployeeInfo>(userver::storages::postgres::kRowTag);

    const auto head_employee_id =
        employee_info.head_id.value_or(user_id);

    auto head_info =
        trx.Execute(
               "SELECT name, surname, patronymic, job_position "
               "FROM working_day_" +
                   company_id +
                   ".employees "
               "WHERE id = $1 ",
               head_employee_id)
            .AsSingleRow<HeadInfo>(userver::storages::postgres::kRowTag);

    auto head_template = trx.Execute(
                              "SELECT head_template "
                              "FROM wd_general.companies "
                              "WHERE id = $1 ",
                              company_id)
                              .AsSingleRow<HeadTemplate>(
                                  userver::storages::postgres::kRowTag)
                              .head_template;

    trx.Commit();

    auto file_key = userver::utils::generators::GenerateUuid();

    PyserviceDocumentGenerateRequest link_request;
    link_request.action_type = request_body.type;
    link_request.request_type = "create";
    link_request.employee_id = user_id;
    link_request.employee_name = employee_info.name;
    link_request.employee_surname = employee_info.surname;
    link_request.subcompany = employee_info.subcompany;
    link_request.company_id = company_id;
    link_request.head_name = head_info.name;
    link_request.head_surname = head_info.surname;
    link_request.start_date = userver::utils::datetime::Timestring(
        request_body.start_date, kTz, "%d.%m.%Y");
    link_request.end_date = userver::utils::datetime::Timestring(
        request_body.end_date, kTz, "%d.%m.%Y");
    link_request.employee_patronymic = employee_info.patronymic;
    link_request.head_patronymic = head_info.patronymic;
    link_request.employee_position = employee_info.job_position;
    link_request.head_position = head_info.job_position;
    if (request_body.params.has_value()) {
      link_request.params = request_body.params.value();
    }
    link_request.head_template = head_template;

    auto resp = http_client_.CreateRequest()
                    .post(pyservice_url_ + "?file_key=" + file_key)
                    .data(link_request.ToJsonString())
                    .retry(2)
                    .timeout(std::chrono::milliseconds{5000})
                    .perform();

    resp->raise_for_status();

    auto action_name = ActionTypeToName(request_body.type);
    auto document_name = "Запрос на " + action_name.value() + " " +
                         employee_info.surname + " " + employee_info.name +
                         " " +
                         userver::utils::datetime::Timestring(
                             request_body.start_date, kTz, "%d.%m.%Y") +
                         " - " +
                         userver::utils::datetime::Timestring(
                             request_body.end_date, kTz, "%d.%m.%Y");
    file_key += ".pdf";

    std::vector<DocumentsChainMetadataItemPg> chain_metadata_pg{
        DocumentsChainMetadataItemPg{user_id, 0, 1},
        DocumentsChainMetadataItemPg{head_employee_id, 0, 0},
    };

    trx = pg_cluster_->Begin(
        "documents_generate_from_template_write",
        userver::storages::postgres::ClusterHostType::kMaster,
        {});

    trx.Execute("INSERT INTO working_day_" + company_id +
                    ".documents(id, name, sign_required, type, author_id, "
                    "chain_metadata_new) "
                    "VALUES($1, $2, $3, $4, $5, $6)",
                file_key, document_name, 0, "employee_request", user_id,
                chain_metadata_pg);

    trx.Execute("INSERT INTO working_day_" + company_id +
                    ".actions(id, type, user_id, start_date, "
                    "end_date, status, document_id) "
                    "VALUES($1, $2, $3, $4, $5, $6, $7) "
                    "ON CONFLICT (id) "
                    "DO NOTHING",
                action_id, request_body.type, user_id, request_body.start_date,
                request_body.end_date, action_status, file_key);

    trx.Execute(
        "INSERT INTO working_day_" + company_id +
            ".employee_document "
            "(employee_id, document_id, signed) "
            "VALUES ($1, $2, $3), ($4, $2, $5) "
            "ON CONFLICT DO NOTHING",
        user_id, file_key, true, head_employee_id, false);

    auto notification_id = userver::utils::generators::GenerateUuid();
    trx.Execute("INSERT INTO working_day_" + company_id +
                    ".notifications(id, type, text, user_id, "
                    "sender_id, action_id, document_id) "
                    "VALUES($1, $2, $3, $4, $5, $6, $7) "
                    "ON CONFLICT (id) "
                    "DO NOTHING",
                notification_id, "vacation_request", notification_text.value(),
                head_employee_id, user_id, action_id, file_key);

    trx.Commit();

    GenerateFromTemplateResponse result;
    result.document_id = file_key;
    result.download_link = utils::s3_presigned_links::GenerateDocumentPresignedLink(
        file_key, utils::s3_presigned_links::Download, is_testing_);
    return result.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
  bool is_testing_{false};
};

}  // namespace

void AppendDocumentsGenerateFromTemplate(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsGenerateFromTemplateHandler>();
}

}  // namespace views::v1::documents::generate_from_template
