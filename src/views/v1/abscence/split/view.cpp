#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

using json = nlohmann::json;

namespace views::v1::abscence::split {

const std::string tz = "UTC";

namespace {

class AbscenceSplitRequest {
 public:
  AbscenceSplitRequest(const std::string& body) {
    using namespace userver::utils::datetime;
    auto j = json::parse(body);
    split_date = Stringtime(Timestring(Stringtime(j["split_date"], tz, "%Y-%m-%dT%H:%M:%E6S"), tz, "%Y-%m-%d"), tz, "%Y-%m-%d");
    action_id = j["action_id"];
  }

  userver::storages::postgres::TimePoint split_date;
  std::string action_id;
};

class AbscenceSplitResponse {
 public:
  std::string ToJSON() const {
    nlohmann::json j;
    j["first_action_id"] = first_action_id;
    j["second_action_id"] = second_action_id;
    return j.dump();
  }

  std::string first_action_id, second_action_id;
};

class HeadId {
 public:
  std::optional<std::string> head_id;
};

class UserAction {
 public:
  std::string id, type;
  userver::storages::postgres::TimePoint start_date, end_date;
  std::optional<std::string> status;
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

class AbscenceSplitHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-abscence-split";

  AbscenceSplitHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    AbscenceSplitRequest request_body(request.RequestBody());
    auto user_id = ctx.GetData<std::string>("user_id");

    std::string notification_text = "Unknown notification";

    using namespace std::literals::chrono_literals;
    auto trx = pg_cluster_->Begin(
        "split_abscence",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    auto action_to_split = trx.Execute(
        "SELECT id, type, start_date, end_date, status "
        "FROM working_day.actions "
        "WHERE id = $1",
        request_body.action_id)
      .AsSingleRow<UserAction>(userver::storages::postgres::kRowTag);
    auto action_status = action_to_split.status;
    
    if (request_body.split_date < action_to_split.start_date || request_body.split_date > action_to_split.end_date) {
      trx.Rollback();
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Selected date is not between action dates"}.ToJSON();
    }
    
    auto first_action_id = userver::utils::generators::GenerateUuid();
    auto second_action_id = userver::utils::generators::GenerateUuid();

    auto result = trx.Execute(
        "INSERT INTO working_day.actions(id, type, user_id, start_date, "
        "end_date, status, underlying_action_id) "
        "VALUES($1, $2, $3, $4, $5, $6, $7), "
        "($8, $9, $10, $11, $12, $13, $14) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        first_action_id, action_to_split.type , user_id, action_to_split.start_date,
        request_body.split_date + 1439min, action_status, request_body.action_id,
        second_action_id, action_to_split.type , user_id, request_body.split_date + 1440min,
        action_to_split.end_date, action_status, request_body.action_id);

    result = trx.Execute(
        "UPDATE working_day.actions "
        "SET blocking_actions_ids = $2 "
        "WHERE id = $1",
        request_body.action_id, std::vector{first_action_id, second_action_id});

    auto notification_id = userver::utils::generators::GenerateUuid();
    result = trx.Execute(
        "INSERT INTO working_day.notifications(id, type, text, user_id, "
        "sender_id, action_id) "
        "VALUES($1, $2, $3, $4, $5, $6) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        notification_id, action_to_split.type + "_split",
        "Ваш отпуск с " + userver::utils::datetime::Timestring(action_to_split.start_date, "UTC", "%d.%m.%Y") + " был разделен на две части.",
        user_id, user_id, request_body.action_id);

    /* TODO
    auto head_id =
        trx.Execute(
               "SELECT head_id "
               "FROM working_day.employees "
               "WHERE id = $1",
               user_id)
            .AsSingleRow<HeadId>(userver::storages::postgres::kRowTag)
            .head_id;

    if (request_body.type == "vacation") {
      notification_text =
          "Ваш сотрудник запросил отпуск c " +
          userver::utils::datetime::Timestring(request_body.start_date, tz, "%d.%m.%Y") +
          " по " + userver::utils::datetime::Timestring(request_body.end_date, tz, "%d.%m.%Y") +
          ". Подтвердите или отклоните его.";
    }

    if (request_body.type == "vacation") {
      auto notification_id = userver::utils::generators::GenerateUuid();
      result = trx.Execute(
          "INSERT INTO working_day.notifications(id, type, text, user_id, "
          "sender_id, action_id) "
          "VALUES($1, $2, $3, $4, $5, $6) "
          "ON CONFLICT (id) "
          "DO NOTHING",
          notification_id, request_body.type + "_request", notification_text,
          head_id.value_or(user_id), user_id, action_id);
    } */

    trx.Commit();

    AbscenceSplitResponse response{first_action_id, second_action_id};
    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAbscenceSplit(userver::components::ComponentList& component_list) {
  component_list.Append<AbscenceSplitHandler>();
}

}  // namespace views::v1::abscence::split