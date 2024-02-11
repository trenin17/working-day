#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include "userver/utils/async.hpp"

#include "core/reverse_index/view.hpp"

namespace views::v1::clear_tasks {

namespace {

class ClearTasksHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-clear-tasks";

  ClearTasksHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext&) const override {

    views::v1::reverse_index::ClearTasks();

    return "";
  }

private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendClearTasks(userver::components::ComponentList& component_list) {
  component_list.Append<ClearTasksHandler>();
}

}  // namespace views::v1::add_company