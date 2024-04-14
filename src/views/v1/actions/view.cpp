#define V1_ACTIONS

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include <definitions/all.hpp>
#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::actions {

namespace {

class ActionsHandler final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-actions";

  ActionsHandler(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& user_id = ctx.GetData<std::string>("user_id");
    ActionsRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, type, start_date, end_date, status, blocking_actions_ids "
        "FROM working_day.actions "
        "WHERE (user_id = $1 AND start_date >= $2 AND start_date <= $3) "
        "OR (user_id = $1 AND end_date >= $2 AND end_date <= $3)",
        request_body.employee_id.value_or(user_id), request_body.from,
        request_body.to);

    ActionsResponse response;
    response.actions = result.AsContainer<std::vector<UserAction>>(
        userver::storages::postgres::kRowTag);

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendActions(userver::components::ComponentList& component_list) {
  component_list.Append<ActionsHandler>();
}

}  // namespace views::v1::actions