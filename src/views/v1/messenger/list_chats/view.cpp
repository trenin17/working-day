#define V1_MESSENGER_INFO

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "definitions/all.hpp"

#include <string>

namespace views::v1::messenger::list_chats {

namespace {

struct ListedChatInfo {
  std::string chat_id;
  std::string chat_name;
  std::optional<userver::storages::postgres::TimePoint> timestamp;
  std::optional<std::string> sender_id;
  std::optional<std::string> content;
};

class ListChatsHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-messenger-list-chats";

  ListChatsHandler(
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

    const auto& employee_id = request.GetArg("employee_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto query = fmt::format(
        "SELECT DISTINCT ON (mc.chat_id) "
        "mc.chat_id, mc.chat_name, m.timestamp, "
        "m.sender_id, m.content "
        "FROM working_day_{0}.employee_chats ec "
        "JOIN working_day_{0}.messenger_chats mc ON ec.chat_id = mc.chat_id "
        "LEFT JOIN working_day_{0}.messages m ON m.chat_id = mc.chat_id "
        "WHERE ec.employee_id = $1 "
        "ORDER BY mc.chat_id, m.timestamp DESC;",
        company_id);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster, std::move(query),
        employee_id);

    auto chats_info = result.AsContainer<std::vector<ListedChatInfo>>(
          userver::storages::postgres::kRowTag);

    MessengerListAllChats listed_chats;
    listed_chats.chats.reserve(chats_info.size());
    for (auto& info : chats_info) {
      MessengerListedChatInfo chat_info;

      chat_info.chat_id = info.chat_id;
      chat_info.chat_name = std::move(info.chat_name);

      MessengerMessage message;
      if (info.sender_id.has_value() && info.content.has_value() && info.timestamp.has_value()) {
        message.chat_id = std::move(info.chat_id);
        message.sender_id = std::move(*info.sender_id);
        message.content = MessengerMessageContent(std::move(*info.content));
        message.timestamp = std::move(*info.timestamp);
      }
      chat_info.last_message = std::move(message);

      listed_chats.chats.push_back(std::move(chat_info));
    }

    return listed_chats.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendListChats(userver::components::ComponentList& component_list) {
  component_list.Append<ListChatsHandler>();
}

}  // namespace views::v1::messenger::list_chats