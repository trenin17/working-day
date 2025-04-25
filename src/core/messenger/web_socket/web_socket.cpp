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


    void Handle(userver::server::websocket::WebSocketConnection& chat, userver::server::request::RequestContext& context) const override {
        std::string company_id = "first";
        std::string user_id;

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

        try {
            while (!userver::engine::current_task::ShouldCancel()) {
                userver::server::websocket::Message incoming_message;
                chat.Recv(incoming_message);

                if (incoming_message.close_status) break;

                MessengerMessage protocol_message;
                protocol_message.ParseRegisteredFields(incoming_message.data);

                user_id = protocol_message.sender_id;

                auto j = json::parse(incoming_message.data);
                if (j["content"].contains("company_id")) {
                    company_id = j["content"]["company_id"];
                }

                core::queue_manager::QueueManager::GetInstance().RegisterQueue(company_id, user_id, connection_queue);

                PersistMessage(company_id, protocol_message);

                BroadcastMessage(company_id, GetChatMembers(company_id, protocol_message.chat_id), incoming_message);
                // BroadcastMessage(company_id, {EmployeeId("test_id1"), EmployeeId("test_id2")}, incoming_message);
            }
        } catch (const std::exception& ex) {
            LOG_ERROR() << "WebSocket error: " << ex.what();
        }

        if (send_task) {
            send_task->RequestCancel();
            send_task->Get();
        }

        if (!user_id.empty()) {
            core::queue_manager::QueueManager::GetInstance().UnregisterQueue(company_id, user_id);
        }
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