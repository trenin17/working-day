#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::notifications {

namespace {

class ListEmployee {
 public:
  json ToJSONObject() const {
    json j;
    j["id"] = id;
    j["name"] = name;
    j["surname"] = surname;
    if (patronymic) {
      j["patronymic"] = patronymic.value();
    }
    if (photo_link) {
      j["photo_link"] = utils::s3_presigned_links::GeneratePhotoPresignedLink(
          photo_link.value(), utils::s3_presigned_links::LinkType::Download);
    }

    return j;
  }

  std::string id, name, surname;
  std::optional<std::string> patronymic, photo_link;
};

class Notification {
 public:
  json ToJSONObject() const {
    json j;
    j["id"] = id;
    j["type"] = type;
    j["text"] = text;
    j["is_read"] = is_read;
    if (sender) {
      j["sender"] = sender.value().ToJSONObject();
    }
    if (action_id) {
      j["action_id"] = action_id.value();
    }
    j["created"] = userver::utils::datetime::Timestring(created, "UTC",
                                                        "%Y-%m-%dT%H:%M:%E6S");

    return j;
  }

  std::string id, type, text;
  bool is_read;
  std::optional<ListEmployee> sender;
  std::optional<std::string> action_id;
  userver::storages::postgres::TimePoint created;
};

class NotificationsResponse {
 public:
  std::string ToJSON() const {
    json j;
    j["notifications"] = json::array();
    for (const auto& notification : notifications) {
      j["notifications"].push_back(notification.ToJSONObject());
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
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& user_id = ctx.GetData<std::string>("user_id");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT working_day.notifications.id, type, text, is_read, ROW "
        "(working_day.employees.id, working_day.employees.name, "
        "working_day.employees.surname, working_day.employees.patronymic, "
        "working_day.employees.photo_link), "
        "action_id, created "
        "FROM working_day.notifications "
        "LEFT JOIN working_day.employees "
        "ON working_day.employees.id = working_day.notifications.sender_id "
        "WHERE user_id = $1 "
        "LIMIT 100",
        user_id);

    NotificationsResponse response{
        result.AsContainer<std::vector<Notification>>(
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