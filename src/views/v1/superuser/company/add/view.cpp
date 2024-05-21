#define V1_SUPERUSER_COMPANY_ADD

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "definitions/all.hpp"

namespace views::v1::superuser::company::add {

namespace {

class SuperuserCompanyAddHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-superuser-company-add";

  SuperuserCompanyAddHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()),
        db_address_(config["db"].As<std::string>()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    SuperuserCompanyAddRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    LOG_WARNING() << "Current directory: " << std::filesystem::current_path().string();
    auto shell_command = "/home/developer/diploma/scripts/setup_comapny_db.sh working_day_" + request_body.company_id + " " + db_address_;
    auto err_code = system(shell_command.c_str());

    if (err_code != 0) {
      LOG_ERROR() << "Failed to setup company database";
    }

    return "";
  }

static userver::yaml_config::Schema GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Superuser add company handler schema
additionalProperties: false
properties:
    db:
        type: string
        description: database address
)");
}

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  std::string db_address_;
};

}  // namespace

void AppendSuperuserCompanyAdd(userver::components::ComponentList& component_list) {
  component_list.Append<SuperuserCompanyAddHandler>();
}

}  // namespace views::v1::superuser::company::add
