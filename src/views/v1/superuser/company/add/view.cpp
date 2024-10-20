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

    std::string script_path = std::filesystem::current_path().string() +
                              "/scripts/setup_company_db.sh";
    if (!std::filesystem::exists(script_path)) {
      script_path = std::filesystem::current_path().parent_path().string() +
                    "/scripts/setup_company_db.sh";
    }
    auto shell_command = script_path + " working_day_" +
                         request_body.company_id + " '" +
                         request_body.company_name + "' " + db_address_;
    auto err_code = system(shell_command.c_str());

    if (err_code != 0) {
      LOG_ERROR() << "Failed to setup company database";
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kInternalServerError);
      return ErrorMessage{"Failed to set up database"}.ToJsonString();
    }

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO wd_general.companies(id, name) VALUES($1, $2)",
        request_body.company_id, request_body.company_name);

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "INSERT INTO working_day_" + request_body.company_id +
                             " .teams "
                             "(id, name) VALUES($1, $2)",
                         "default_team", "Default team");

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

void AppendSuperuserCompanyAdd(
    userver::components::ComponentList& component_list) {
  component_list.Append<SuperuserCompanyAddHandler>();
}

}  // namespace views::v1::superuser::company::add
