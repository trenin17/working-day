#pragma once
#include <userver/utest/using_namespace_userver.hpp>

#include <userver/components/component.hpp>
#include <userver/engine/io/socket.hpp>
#include <userver/formats/json.hpp>
#include <userver/components/component_base.hpp>
#include <userver/engine/task/task.hpp>
#include <userver/formats/json/value.hpp>
#include "../ML/ml_process_manager.hpp"

namespace analitics::containers {
struct RequestResponseData;
}

namespace analitics::ml_client {

class MlClient final : public components::ComponentBase {
public:
    static constexpr std::string_view kName = "ml-tcp-client";
    
    MlClient(const components::ComponentConfig& config,
        const components::ComponentContext& context);
        
    void SendToML(const std::string& context);
    
    static bool ShouldSend(const analitics::containers::RequestResponseData&);
    
    static userver::yaml_config::Schema GetStaticConfigSchema();

    ~MlClient();
private:
    bool is_connected_;
    void ListenLoop();
    engine::io::Socket python_ws_client_;
    engine::Task listen_task_;
    MlProcessManager& deps_;
};

};