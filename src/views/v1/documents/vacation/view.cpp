#define V1_DOCUMENTS_VACATION

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

#include "definitions/all.hpp"

using json = nlohmann::json;

namespace views::v1::documents::vacation {

namespace {

class ActionInfo {
 public:
  std::string employee_id, type;
  userver::storages::postgres::TimePoint start_date, end_date;
  std::vector<std::string> blocking_actions_ids;
};

class EmployeeInfo {
 public:
  std::string name, surname;
  std::optional<std::string> patronymic, head_id, position;
};

class HeadInfo {
 public:
  std::string name, surname;
  std::optional<std::string> patronymic, position;
};

class DocumentsVacationHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-vacation";

  DocumentsVacationHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()),
        http_client_(
            component_context.FindComponent<userver::components::HttpClient>()
                .GetHttpClient()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    auto action_id = request.GetArg("action_id");
    auto request_type = request.GetArg("request_type");
    if (request_type.empty()) {
      request_type = "create";
    }
    auto user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto trx = pg_cluster_->Begin(
        "documents_vacation",
        userver::storages::postgres::ClusterHostType::kMaster,
        {});  // TODO: change to slave read tx

    auto action_info =
        trx.Execute(
               "SELECT user_id, type, start_date, end_date, "
               "blocking_actions_ids "
               "FROM working_day_" +
                   company_id +
                   ".actions "
                   "WHERE id = $1",
               action_id)
            .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);

    if (request_type == "split" &&
        action_info.blocking_actions_ids.size() != 2) {
      trx.Rollback();
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Action was not split or was split the wrong way"}
          .ToJsonString();
    }

    if (action_info.type != "vacation") {
      trx.Rollback();
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Wrong action type"}.ToJsonString();
    }

    LOG_INFO() << "Employee id: (" << action_info.employee_id << ")";

    auto employee_info =
        trx.Execute(
               "SELECT name, surname, patronymic, head_id, position "
               "FROM working_day_" +
                   company_id +
                   ".employees "
                   "WHERE id = $1 ",
               action_info.employee_id)
            .AsSingleRow<EmployeeInfo>(userver::storages::postgres::kRowTag);

    LOG_INFO() << "Head id: ("
               << employee_info.head_id.value_or(action_info.employee_id)
               << ")";

    auto head_info =
        trx.Execute(
               "SELECT name, surname, patronymic, position "
               "FROM working_day_" +
                   company_id +
                   ".employees "
                   "WHERE id = $1 ",
               employee_info.head_id.value_or(action_info.employee_id))
            .AsSingleRow<HeadInfo>(userver::storages::postgres::kRowTag);

    LinkRequest link_request;
    link_request.request_type = request_type;
    link_request.employee_name = employee_info.name;
    link_request.employee_surname = employee_info.surname;
    link_request.head_name = head_info.name;
    link_request.head_surname = head_info.surname;
    link_request.start_date = userver::utils::datetime::Timestring(
        action_info.start_date, "UTC", "%d.%m.%Y");
    link_request.end_date = userver::utils::datetime::Timestring(
        action_info.end_date, "UTC", "%d.%m.%Y"),
    link_request.employee_patronymic = employee_info.patronymic;
    link_request.head_patronymic = head_info.patronymic;
    link_request.employee_position = employee_info.position;
    link_request.head_position = head_info.position;

    if (request_type == "split") {
      auto first_action =
          trx.Execute(
                 "SELECT user_id, type, start_date, end_date, "
                 "blocking_actions_ids "
                 "FROM working_day_" +
                     company_id +
                     ".actions "
                     "WHERE id = $1",
                 action_info.blocking_actions_ids[0])
              .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);
      auto second_action =
          trx.Execute(
                 "SELECT user_id, type, start_date, end_date, "
                 "blocking_actions_ids "
                 "FROM working_day_" +
                     company_id +
                     ".actions "
                     "WHERE id = $1",
                 action_info.blocking_actions_ids[1])
              .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);

      using namespace userver::utils::datetime;
      link_request.first_start_date =
          Timestring(first_action.start_date, "UTC", "%d.%m.%Y");
      link_request.first_end_date =
          Timestring(first_action.end_date, "UTC", "%d.%m.%Y");
      link_request.second_start_date =
          Timestring(second_action.start_date, "UTC", "%d.%m.%Y");
      link_request.second_end_date =
          Timestring(second_action.end_date, "UTC", "%d.%m.%Y");
    }

    trx.Commit();

    auto file_key = userver::utils::generators::GenerateUuid();
    auto response = http_client_.CreateRequest()
                        .post(
                            "http://python-service:3000/"
                            "document/generate?file_key=" +
                            file_key)
                        .data(link_request.ToJsonString())
                        .retry(2)  // retry once in case of error
                        .timeout(std::chrono::milliseconds{5000})
                        .perform();  // start performing the request
    response->raise_for_status();

    auto document_name = "Запрос на отпуск " + employee_info.surname + " " +
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
                             "sign_required, type) "
                             "VALUES($1, $2, $3, $4)",
                         file_key, document_name, true, "employee_request");

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".employee_document "
            "(employee_id, document_id, signed) "
            "VALUES ($1, $2, $3), ($4, $2, $3) "
            "ON CONFLICT DO NOTHING",
        action_info.employee_id, file_key, true,
        employee_info.head_id.value_or(action_info.employee_id));

    return response->body();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
};

}  // namespace

void AppendDocumentsVacation(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsVacationHandler>();
}

}  // namespace views::v1::documents::vacation
