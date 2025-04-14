#define USE_MESSAGES
#include "web_socket.hpp"

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/websocket/websocket_handler.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/daemon_run.hpp>
#include "userver/utils/async.hpp"
#include <userver/concurrent/mpsc_queue.hpp>
#include <userver/logging/log.hpp>

#include <iostream>
#include "definitions/all.hpp"

namespace websocket {

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
        userver::server::websocket::Message incoming_message;
        // auto company_id = context.GetData<std::string>("company_id");
        std::string company_id = "first";
        while (!userver::engine::current_task::ShouldCancel()) {
            // Receiving
            chat.Recv(incoming_message);               // throws on closed/dropped connection
            if (incoming_message.close_status) break;  // explicit close if any
            LOG_INFO() << incoming_message.data;

            MessengerMessage protocol_message;
            protocol_message.ParseRegisteredFields(incoming_message.data);

            PersistMessage(company_id, protocol_message);

            CreateSenderIfNotExists(protocol_message.sender_id);

            BroadcastMessage(GetChatMembers(company_id, protocol_message.sender_id), incoming_message);

            // Sending
            userver::server::websocket::Message outgoing_message;
            if (queue_->GetConsumer().Pop(outgoing_message)) {
                chat.Send(outgoing_message); // throws on closed/dropped connection
            }
        }
        if (incoming_message.close_status) chat.Close(*incoming_message.close_status);
    }

private:
    void BroadcastMessage(const std::vector<EmployeeId>& employee_ids, userver::server::websocket::Message message) const {
        for (const auto& IdStruct: employee_ids) {
            auto queue = GetQueueFor(IdStruct.id);
            if (queue) {
                auto producer_task = userver::utils::Async("producer", [queue, message]() mutable {
                    if (!(*queue).GetProducer().Push(std::move(message))) {
                        LOG_WARNING() << "Failed to push message";
                    }
                });
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

    void CreateSenderIfNotExists(const std::string& sender_id) const {
        std::lock_guard<std::mutex> lock(queues_mutex_);
        if (!queues_.contains(sender_id)) {
            queue_ = userver::concurrent::MpscQueue<userver::server::websocket::Message>::Create();
            queues_[sender_id] = queue_;
        }
    }

    void PersistMessage(const std::string& company_id, const MessengerMessage& message) const {
        auto query = fmt::format(
            "INSERT INTO working_day_{0}.messages(chat_id, timestamp, sender_id, content) "
            "VALUES($1, $2, $3, $4) ",
            company_id);

        auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster, query,
            message.chat_id, message.timestamp, message.sender_id, message.content.content);
    }

    static std::shared_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>>
    GetQueueFor(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(queues_mutex_);
        auto it = queues_.find(user_id);
        if (it != queues_.end()) {
            if (auto queue = it->second.lock()) {
                return queue;
            } else {
                queues_.erase(user_id);
            }
        }
        return nullptr;
    }

private:
    static std::mutex queues_mutex_;
    static std::unordered_map<std::string, std::weak_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>>> queues_;
    mutable std::shared_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>> queue_;
    userver::storages::postgres::ClusterPtr pg_cluster_;
};

std::mutex websocket::WebsocketsHandler::queues_mutex_;
std::unordered_map<std::string, std::weak_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>>>
    websocket::WebsocketsHandler::queues_;

void AppendWebSocket(userver::components::ComponentList& component_list) {
  component_list.Append<WebsocketsHandler>();
}


}  // namespace websocket