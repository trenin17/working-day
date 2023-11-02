#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::actions {

namespace {

class ActionsRequest {
 public:
  ActionsRequest(const std::string& body) {
    auto j = json::parse(body);
    from = userver::utils::datetime::Stringtime(
        j["from"], "UTC",
        "%Y-%m-%dT%H:%M:%E6S");
    to = userver::utils::datetime::Stringtime(j["to"], "UTC",
                                              "%Y-%m-%dT%H:%M:%E6S");
    if (j.contains("employee_id")) {
      employee_id = j["employee_id"];
    }
  }

  userver::storages::postgres::TimePoint from, to;
  std::optional<std::string> employee_id;
};

class UserAction {
 public:
  json ToJSONObject() const {
    json j;
    j["id"] = id;
    j["type"] = type;
    j["start_date"] = userver::utils::datetime::Timestring(
        start_date, "UTC", "%Y-%m-%dT%H:%M:%E6S");
    j["end_date"] = userver::utils::datetime::Timestring(end_date, "UTC",
                                                         "%Y-%m-%dT%H:%M:%E6S");
    if (status) {
      j["status"] = status.value();
    }
    j["blocking_actions_ids"] = blocking_actions_ids;

    return j;
  }

  std::string id, type;
  userver::storages::postgres::TimePoint start_date, end_date;
  std::optional<std::string> status;
  std::vector<std::string> blocking_actions_ids;
};

class ActionsResponse {
 public:
  std::string ToJSON() const {
    json j;
    j["actions"] = json::array();
    for (const auto& action : actions) {
      j["actions"].push_back(action.ToJSONObject());
    }
    return j.dump();
  }

  std::vector<UserAction> actions;
};

class ActionsHandler final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-actions";

  ActionsHandler(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    const auto& user_id = ctx.GetData<std::string>("user_id");
    ActionsRequest request_body(request.RequestBody());

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, type, start_date, end_date, status, blocking_actions_ids "
        "FROM working_day.actions "
        "WHERE (user_id = $1 AND start_date >= $2 AND start_date <= $3) "
        "OR (user_id = $1 AND end_date >= $2 AND end_date <= $3)",
        request_body.employee_id.value_or(user_id), request_body.from, request_body.to);

    ActionsResponse response{result.AsContainer<std::vector<UserAction>>(
        userver::storages::postgres::kRowTag)};

    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendActions(userver::components::ComponentList& component_list) {
  component_list.Append<ActionsHandler>();
}

}  // namespace views::v1::actions