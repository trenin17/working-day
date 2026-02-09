#define USE_MESSAGES
#include "web_socket.hpp"

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/websocket/websocket_handler.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/utils/async.hpp>
#include <userver/engine/async.hpp>
#include <userver/concurrent/mpsc_queue.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <chrono>

#include "definitions/all.hpp"
#include "core/messenger/queue_manager/queue_manager.hpp"

using json = nlohmann::json;

namespace core::websocket {

namespace {
    struct EmployeeId {
        std::string id;
    };

    struct HandshakeData {
        std::string user_id;
        std::string company_id;
    };
}

class WebsocketsHandler final : public userver::server::websocket::WebsocketHandlerBase {
public:
    // `kName` is used as the component name in static config
    static constexpr std::string_view kName = "websocket-handler";

    // Component is valid after construction and is able to accept requests
    using WebsocketHandlerBase::WebsocketHandlerBase;

    WebsocketsHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : WebsocketHandlerBase(config, component_context),
          pg_cluster_(
                component_context
                  .FindComponent<userver::components::Postgres>("key-value")
                  .GetCluster()) {}

    bool HandleHandshake(
        const userver::server::http::HttpRequest& request,
        userver::server::http::HttpResponse&,
        userver::server::request::RequestContext& context) const override {

        std::string token;

        // Try Authorization header first
        const auto& auth_value = request.GetHeader("Authorization");
        if (!auth_value.empty()) {
            const auto sep = auth_value.find(' ');
            if (sep != std::string::npos &&
                std::string_view{auth_value.data(), sep} == "Bearer") {
                token = auth_value.substr(sep + 1);
            }
        }

        // Fallback to query parameter (for browser WebSocket clients)
        if (token.empty()) {
            const auto& token_arg = request.GetArg("token");
            if (!token_arg.empty()) {
                token = token_arg;
            }
        }

        if (token.empty()) {
            LOG_WARNING() << "WebSocket rejected: no auth token provided";
            return false;
        }

        // Validate token against database
        auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "SELECT user_id, company_id FROM wd_general.auth_tokens WHERE token = $1",
            token);

        if (result.IsEmpty()) {
            LOG_WARNING() << "WebSocket rejected: invalid auth token";
            return false;
        }

        auto row = result[0];
        HandshakeData data;
        data.user_id = row[0].As<std::string>();
        data.company_id = row[1].As<std::string>();

        LOG_INFO() << "WebSocket authenticated: user_id=" << data.user_id
                   << " company_id=" << data.company_id;

        context.SetUserData(std::move(data));
        return true;
    }

    void Handle(userver::server::websocket::WebSocketConnection& chat, userver::server::request::RequestContext& context) const override {
        // Get authenticated user info from handshake
        const auto& auth = context.GetUserData<HandshakeData>();
        const auto& user_id = auth.user_id;
        const auto& company_id = auth.company_id;

        auto connection_queue = userver::concurrent::MpscQueue<userver::server::websocket::Message>::Create();

        std::shared_ptr<userver::engine::TaskWithResult<void>> send_task;

        {
            send_task = std::make_shared<userver::engine::TaskWithResult<void>>(
                userver::engine::CriticalAsyncNoSpan([connection_queue, &chat]() {
                    auto consumer = connection_queue->GetConsumer();
                    while (!userver::engine::current_task::ShouldCancel()) {
                        userver::server::websocket::Message outgoing_message;
                        if (consumer.Pop(outgoing_message)) {
                            try {
                                chat.Send(outgoing_message);
                                LOG_INFO() << "Message sent to client";
                            } catch (const std::exception& ex) {
                                LOG_ERROR() << "Error sending: " << ex.what();
                            }
                        }
                    }
                })
            );
        }

        // Register queue ONCE on connect (fixes ISSUE #8)
        core::queue_manager::QueueManager::GetInstance().RegisterQueue(company_id, user_id, connection_queue);

        try {
            while (!userver::engine::current_task::ShouldCancel()) {
                userver::server::websocket::Message incoming_message;
                chat.Recv(incoming_message);

                if (incoming_message.close_status) break;

                auto j = json::parse(incoming_message.data);

                MessengerMessage protocol_message;
                protocol_message.chat_id = j.value("chat_id", "");
                protocol_message.sender_id = user_id;
                if (j.contains("content") && j["content"].contains("content")) {
                    protocol_message.content = MessengerMessageContent(j["content"]["content"].get<std::string>());
                }

                PersistMessage(company_id, protocol_message);

                // Reconstruct broadcast message with authenticated sender_id
                j["sender_id"] = user_id;
                userver::server::websocket::Message broadcast_msg;
                broadcast_msg.data = j.dump();
                broadcast_msg.is_text = incoming_message.is_text;

                BroadcastMessage(company_id, GetChatMembers(company_id, protocol_message.chat_id), broadcast_msg);
            }
        } catch (const std::exception& ex) {
            LOG_ERROR() << "WebSocket error: " << ex.what();
        }

        if (send_task) {
            send_task->RequestCancel();
            send_task->Get();
        }

        core::queue_manager::QueueManager::GetInstance().UnregisterQueue(company_id, user_id);
    }

private:
    void BroadcastMessage(const std::string& company_id, const std::vector<EmployeeId>& employee_ids, userver::server::websocket::Message message) const {
        for (const auto& IdStruct: employee_ids) {
            auto queue = core::queue_manager::QueueManager::GetInstance().GetQueueFor(company_id, IdStruct.id);
            if (queue) {
                auto msg_copy = message;
                auto producer = (*queue).GetProducer();
                if (!producer.Push(std::move(msg_copy))) {
                    LOG_WARNING() << "Failed to push message";
                }
            }
        }
    }

    std::vector<EmployeeId> GetChatMembers(const std::string& company_id, const std::string& chat_id) const {
        auto query = fmt::format(
            "SELECT employee_id "
            "FROM working_day_{0}.employee_chats "
            "WHERE chat_id = $1;",
            company_id);
        auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster, query,
            chat_id);

        return result.AsContainer<std::vector<EmployeeId>>(userver::storages::postgres::kRowTag);
    }

    void PersistMessage(const std::string& company_id, MessengerMessage message) const {
        auto now = std::chrono::system_clock::now();
        userver::storages::postgres::TimePoint pg_time_point{now};
        message.timestamp = pg_time_point;

        auto query = fmt::format(
            "INSERT INTO working_day_{0}.messages(chat_id, timestamp, sender_id, content) "
            "VALUES($1, $2, $3, $4) ",
            company_id);

        auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster, query,
            message.chat_id, message.timestamp.value(), message.sender_id, message.content.content);
    }

private:
    userver::storages::postgres::ClusterPtr pg_cluster_;
};

void AppendWebSocket(userver::components::ComponentList& component_list) {
  component_list.Append<WebsocketsHandler>();
}


}  // namespace websocket
