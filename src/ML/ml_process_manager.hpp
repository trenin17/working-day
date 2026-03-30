#pragma once

#include <userver/components/component.hpp>
#include <userver/components/process_starter.hpp>
#include <userver/engine/subprocess/process_starter.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace analitics {

class MlProcessManager final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "ml-process-manager";

    MlProcessManager(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context);

    ~MlProcessManager() override;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    userver::components::ProcessStarter& process_starter_;
    std::optional<userver::engine::subprocess::ChildProcess> python_process_;
    std::string port_;
};

}  // namespace analitics