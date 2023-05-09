#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::notifications {

namespace {

class Notification {
 public:
  std::string ToJSON() const {
    json j;
    j["id"] = id;
    j["type"] = type;
    j["text"] = text;
    j["is_read"] = is_read;
    if (sender_id) {
      j["sender_id"] = sender_id.value();
    }
    if (action_id) {
      j["action_id"] = action_id.value();
    }
    j["created"] = userver::utils::datetime::Timestring(created, "UTC", "%Y-%m-%dT%H:%M:%E6S");

    return j.dump();
  }

  std::string id, type, text;
  bool is_read;
  std::optional<std::string> sender_id, action_id;
  userver::storages::postgres::TimePoint created;
};

class NotificationsResponse {
 public:
  std::string ToJSON() const {
    json j = json::array();
    for (const auto& notification : notifications) {
      j.push_back(notification.ToJSON());
    }
    return j.dump();
  }

  std::vector<Notification> notifications;
};

class NotificationsHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-notifications";

  NotificationsHandler(
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
    const auto& user_id = ctx.GetData<std::string>("user_id");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, type, text, is_read, sender_id, action_id, created "
        "FROM working_day.notifications "
        "WHERE user_id = $1",
        user_id);

    NotificationsResponse response{result.AsContainer<std::vector<Notification>>(
        userver::storages::postgres::kRowTag)};

    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendNotifications(userver::components::ComponentList& component_list) {
  component_list.Append<NotificationsHandler>();
}

}  // namespace views::v1::notifications