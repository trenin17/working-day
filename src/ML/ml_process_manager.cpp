#include "ml_process_manager.hpp"
#include <csignal>
#include <userver/logging/log.hpp>
#include <userver/engine/subprocess/child_process.hpp>

namespace analitics {

MlProcessManager::MlProcessManager(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      process_starter_(context.FindComponent<userver::components::ProcessStarter>()),
      port_(config["ml_websocket_port"].As<std::string>("65432")) {

    LOG_INFO() << "Starting Python ML WebSocket server from venv on port " << port_ << "...";

    const std::string python_bin = std::string(CMAKE_BINARY_DIR) + "/.venv-ml/bin/python";
    const std::string script_path = std::string(CMAKE_SOURCE_DIR) + "/src/ML/main.py";

    std::vector<std::string> args = {
        script_path,
        port_
    };

    userver::engine::subprocess::ExecOptions options;

    try {
        python_process_ = process_starter_.Get().Exec(
            python_bin,
            args,
            std::move(options)
        );

        LOG_INFO() << "Python ML process started successfully with PID: " 
                   << python_process_->GetPid();
    } catch (const std::exception& ex) {
        LOG_ERROR() << "Failed to start Python ML service: " << ex.what();
    }
}

MlProcessManager::~MlProcessManager() {
    if (!python_process_) {
        return;
    }

    LOG_INFO() << "Stopping Python ML WebSocket process (PID " 
               << python_process_->GetPid() << ")";

    try {
// that is the bad way, but in userver 2.11 nothing else
        python_process_->SendSignal(SIGTERM);
        
        auto deadline = userver::engine::Deadline::FromDuration(std::chrono::seconds(5));
        auto status = python_process_->WaitUntil(deadline);

        if (status) {
            LOG_INFO() << "Python ML process exited gracefully";
            return;
        }

        LOG_WARNING() << "Python process did not exit in time → sending SIGKILL";
        python_process_->SendSignal(SIGKILL);
        python_process_->Wait();
    } catch (const std::exception& ex) {
        LOG_ERROR() << "Error while stopping Python ML process: " << ex.what();
    }
}

userver::yaml_config::Schema MlProcessManager::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<ComponentBase>(R"(
type: object
description: Manages the Python ML WebSocket subprocess
additionalProperties: false
properties:
    ml_websocket_port:
        type: integer
        description: Port for Python ML WebSocket server
        minimum: 1
        maximum: 65535
        defaultDescription: "65432"
)");
}

} // namespace analitics