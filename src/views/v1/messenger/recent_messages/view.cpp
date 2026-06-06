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

namespace views::v1::messenger::recent_messages {

struct MessageRow {
  std::string chat_id;
  userver::storages::postgres::TimePoint timestamp;
  std::optional<std::string> sender_id;
  std::optional<std::string> content;
};

namespace {

class LoadRecentMessagesHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-messenger-recent-messages";

  LoadRecentMessagesHandler(
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

    const auto& company_id = ctx.GetData<std::string>("company_id");

    LoadRecentMessagesRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    const auto& chat_id = request_body.chat_id;

    auto query = fmt::format(
            "SELECT chat_id, timestamp, sender_id, content "
            "FROM working_day_{0}.messages "
            "WHERE chat_id = $1 "
            "ORDER BY timestamp DESC "
            "LIMIT 100; ",
        company_id);

    auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster, std::move(query),
          chat_id);

    auto messages = result.AsContainer<std::vector<MessageRow>>(
            userver::storages::postgres::kRowTag);

    nlohmann::json jsonArray;
    for (size_t i = 0; i < messages.size(); ++i) {
      MessengerMessage msg;
      msg.chat_id = messages[i].chat_id;
      msg.timestamp = messages[i].timestamp;
      msg.sender_id = messages[i].sender_id.value_or("");
      msg.content = MessengerMessageContent(messages[i].content.value_or(""));
      jsonArray[i] = nlohmann::json::parse(msg.ToJsonString());
    }
    return to_string(jsonArray);
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendRecentMessages(userver::components::ComponentList& component_list) {
  component_list.Append<LoadRecentMessagesHandler>();
}


}  // namespace views::v1::messenger::recent_messages