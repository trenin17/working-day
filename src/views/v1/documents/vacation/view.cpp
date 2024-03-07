#include "view.hpp"

#include <nlohmann/json.hpp>

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

class LinkRequest {
 public:
  std::string ToJSON() {
    json j;
    j["request_type"] = request_type;
    j["employee_name"] = employee_name;
    j["employee_surname"] = employee_surname;
    j["head_name"] = head_name;
    j["head_surname"] = head_surname;
    j["start_date"] = start_date;
    j["end_date"] = end_date;
    if (employee_patronymic) {
      j["employee_patronymic"] = employee_patronymic.value();
    }
    if (head_patronymic) {
      j["head_patronymic"] = head_patronymic.value();
    }
    if (employee_position) {
      j["employee_position"] = employee_position.value();
    }
    if (head_position) {
      j["head_position"] = head_position.value();
    }
    if (first_start_date) {
      j["first_start_date"] = first_start_date.value();
    }
    if (first_end_date) {
      j["first_end_date"] = first_end_date.value();
    }
    if (second_start_date) {
      j["second_start_date"] = second_start_date.value();
    }
    if (second_end_date) {
      j["second_end_date"] = second_end_date.value();
    }

    return j.dump();
  }

  std::string request_type, employee_name, employee_surname, head_name,
      head_surname, start_date, end_date;
  std::optional<std::string> employee_patronymic, head_patronymic,
      employee_position, head_position, first_start_date, first_end_date,
      second_start_date, second_end_date;
};

class ErrorMessage {
 public:
  std::string ToJSON() const {
    json j;
    j["message"] = message;

    return j.dump();
  }

  std::string message;
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

    auto trx = pg_cluster_->Begin(
        "documents_vacation",
        userver::storages::postgres::ClusterHostType::kMaster,
        {});  // TODO: change to slave read tx

    auto action_info =
        trx.Execute(
               "SELECT user_id, type, start_date, end_date, "
               "blocking_actions_ids "
               "FROM working_day.actions "
               "WHERE id = $1",
               action_id)
            .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);

    if (request_type == "split" &&
        action_info.blocking_actions_ids.size() != 2) {
      trx.Rollback();
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Action was not split or was split the wrong way"}
          .ToJSON();
    }

    if (action_info.type != "vacation") {
      trx.Rollback();
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Wrong action type"}.ToJSON();
    }

    LOG_INFO() << "Employee id: (" << action_info.employee_id << ")";

    auto employee_info =
        trx.Execute(
               "SELECT name, surname, patronymic, head_id, position "
               "FROM working_day.employees "
               "WHERE id = $1 ",
               action_info.employee_id)
            .AsSingleRow<EmployeeInfo>(userver::storages::postgres::kRowTag);

    LOG_INFO() << "Head id: ("
               << employee_info.head_id.value_or(action_info.employee_id)
               << ")";

    auto head_info =
        trx.Execute(
               "SELECT name, surname, patronymic, position "
               "FROM working_day.employees "
               "WHERE id = $1 ",
               employee_info.head_id.value_or(action_info.employee_id))
            .AsSingleRow<HeadInfo>(userver::storages::postgres::kRowTag);

    LinkRequest link_request{.request_type = request_type,
                             .employee_name = employee_info.name,
                             .employee_surname = employee_info.surname,
                             .head_name = head_info.name,
                             .head_surname = head_info.surname,
                             .start_date = userver::utils::datetime::Timestring(
                                 action_info.start_date, "UTC", "%d.%m.%Y"),
                             .end_date = userver::utils::datetime::Timestring(
                                 action_info.end_date, "UTC", "%d.%m.%Y"),
                             .employee_patronymic = employee_info.patronymic,
                             .head_patronymic = head_info.patronymic,
                             .employee_position = employee_info.position,
                             .head_position = head_info.position};

    if (request_type == "split") {
      auto first_action =
          trx.Execute(
                 "SELECT user_id, type, start_date, end_date, "
                 "blocking_actions_ids "
                 "FROM working_day.actions "
                 "WHERE id = $1",
                 action_info.blocking_actions_ids[0])
              .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);
      auto second_action =
          trx.Execute(
                 "SELECT user_id, type, start_date, end_date, "
                 "blocking_actions_ids "
                 "FROM working_day.actions "
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

    auto response =
        http_client_.CreateRequest()
            .get(
                "https://functions.yandexcloud.net/"
                "d4emv61q8h6eu2rs2f67?file_key=" +
                userver::utils::generators::
                    GenerateUuid())  // HTTP GET translations_url_ URL
            .data(link_request.ToJSON())
            .retry(2)  // retry once in case of error
            .timeout(std::chrono::milliseconds{5000})
            .perform();  // start performing the request
    response->raise_for_status();

    LOG_INFO() << link_request.ToJSON();

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