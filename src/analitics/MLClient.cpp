#include "MLClient.hpp"
#include <chrono>
#include <cstddef>
#include <userver/logging/log.hpp>
#include <vector>
#include "userver/engine/async.hpp"
#include "userver/engine/deadline.hpp"
#include "userver/engine/io/sockaddr.hpp"
#include "userver/yaml_config/merge_schemas.hpp"
#include <userver/components/component_context.hpp>

namespace analitics::ml_client {
using engine::Deadline;

MlClient::MlClient(const components::ComponentConfig& config,
    const components::ComponentContext& context)
    : ComponentBase(config, context),
     is_connected_(false),
      python_ws_client_(engine::io::AddrDomain::kInet, 
                                  engine::io::SocketType::kStream),
     deps_(context.FindComponent<MlProcessManager>())
{
    auto addr = engine::io::Sockaddr::MakeIPv4LoopbackAddress();
    int port = config["ml_websocket_port"].As<int>(8080);

    addr.SetPort(port);

    LOG_ERROR() << "Connecting to Python ML service on port " << port;
    for (size_t i = 0; i < 50; ++i) {
        try {
            python_ws_client_.Connect(addr, engine::Deadline::FromDuration(std::chrono::seconds(120)));
            is_connected_ = true;
            break;
        } catch (const engine::io::IoException& ex) {

        } catch (const std::exception& ex) {
            LOG_ERROR() << "Failed to connect to Python ML service: " << ex.what();
            throw;    
        }         
    }

    LOG_ERROR() << "Successfully connected to ML service";
    listen_task_ = engine::AsyncNoSpan([this] { ListenLoop(); });
}

MlClient::~MlClient() {
    if (listen_task_.IsValid()) {
        listen_task_.RequestCancel();
        listen_task_.Wait();
    }
}

void MlClient::SendToML(const std::string& request) {
    if (is_connected_) {
        ssize_t sended = python_ws_client_.SendAll(request.data(), request.size(), engine::Deadline::FromDuration(std::chrono::seconds(10)));
        if (sended <= 0) {
            throw;
        }
    }
}

void MlClient::ListenLoop() {
    static constexpr size_t k8KB = 1 << 13;

    while (!engine::current_task::IsCancelRequested()) {
        std::vector<char> buffer(k8KB);
        ssize_t readed = python_ws_client_.ReadAll(
            buffer.data(),
            buffer.size(),
            Deadline{});
        
        if (readed <= 0) {
            throw;
        }

        std::string message(buffer.data(), readed);
    }
}

bool MlClient::ShouldSend(analitics::containers::RequestResponseData const&) {
    return true;
}

userver::yaml_config::Schema MlClient::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<components::ComponentBase>(R"(
type: object
description: ML TCP client component that connects to Python ML service
additionalProperties: false
properties:
    ml_websocket_port:
        type: integer
        description: Port of the Python ML WebSocket server
        minimum: 1
        maximum: 65535
        defaultDescription: "usually 65432"
)");
}

};  // namespace analitics::ml_client