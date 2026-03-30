#include "frontend_websocket.hpp"
#include "containers/request_collector.hpp"

#include <string>
#include <unordered_map>

#include <userver/logging/log.hpp>
#include <userver/server/websocket/websocket_handler.hpp>
#include <userver/components/component.hpp>
#include <userver/concurrent/variable.hpp>
#include <userver/server/websocket/websocket_handler.hpp>

namespace analitics::websocket {

class WebsocketBroadcaster final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "frontend-websocket-broadcaster";

    WebsocketBroadcaster(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context);

    void SendToUid(const std::string& uid, const std::string& message);

    void RegisterConnection(const std::string& uid,
                            userver::server::websocket::WebSocketConnection* connection);

    void UnregisterConnection(const std::string& uid);

private:
    using ConnectionsMap = std::unordered_map<
            std::string,
            userver::server::websocket::WebSocketConnection*>;
            
    userver::concurrent::Variable<ConnectionsMap> connections_;
};

class FrontendWebsocketsHandler final : public server::websocket::WebsocketHandlerBase {
public:
    static constexpr std::string_view kName = "frontend-websocket-handler";

    FrontendWebsocketsHandler(const components::ComponentConfig& config,
                              const components::ComponentContext& context);

    void Handle(userver::server::websocket::WebSocketConnection& ws,
                server::request::RequestContext& ctx) const override;

private:
    containers::RequestCollector& collector_;
    WebsocketBroadcaster& broadcaster_;
};

WebsocketBroadcaster::WebsocketBroadcaster(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context) {}

void WebsocketBroadcaster::RegisterConnection(
    const std::string& uid,
    userver::server::websocket::WebSocketConnection* connection) {

    auto map = connections_.Lock();
    map->emplace(uid, connection);
}

void WebsocketBroadcaster::SendToUid(const std::string& uid, const std::string& message) {
    auto map = connections_.Lock();
    auto it = map->find(uid);
    if (it != map->end()) {
        userver::server::websocket::Message ws_msg;
        ws_msg.data = message;
        ws_msg.is_text = true;
        it->second->Send(ws_msg);
    } else {
        throw;
    }
}

void WebsocketBroadcaster::UnregisterConnection(const std::string& uid) {
    auto map = connections_.Lock();
    map->erase(uid);
}


FrontendWebsocketsHandler::FrontendWebsocketsHandler(
    const components::ComponentConfig& config,
    const components::ComponentContext& context)
    : server::websocket::WebsocketHandlerBase(config, context)
    , collector_(context.FindComponent<containers::RequestCollector>())
    , broadcaster_(context.FindComponent<WebsocketBroadcaster>()){
}

void FrontendWebsocketsHandler::Handle(
    userver::server::websocket::WebSocketConnection& ws,
    server::request::RequestContext& ctx) const
{
    auto uid_opt = ctx.GetDataOptional<std::string>("user_id");
    if (!uid_opt) {
        LOG_WARNING() << "WebSocket connected without user_id in context";
        return;
    }
    const std::string& uid = *uid_opt;

    if (uid.empty()) {
        LOG_WARNING() << "WebSocket connected without uid parameter";
        return;
    }

    broadcaster_.RegisterConnection(uid, &ws);
    
    LOG_INFO() << "Frontend WebSocket registered for uid=" << uid;

    userver::server::websocket::Message message;
    while (!engine::current_task::ShouldCancel()) {
        ws.Recv(message);

        if (message.close_status) {
            break;
        }

        if (!message.is_text) {
            continue;
        }

        collector_.FrontendCollect(message.data);
    }

    broadcaster_.UnregisterConnection(uid);

    if (message.close_status) {
        ws.Close(*message.close_status);
    }
}

void AppendRequestFrontendWebsockets(userver::components::ComponentList& component_list) {
    component_list.Append<FrontendWebsocketsHandler>();
    component_list.Append<WebsocketBroadcaster>();
}


}  // namespace analitics::websocket