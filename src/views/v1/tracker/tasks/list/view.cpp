#define V1_TRACKER_TASKS_LIST

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>

namespace views::v1::tracker::tasks::list {

namespace {

class TrackerTasksListHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-list";

    TrackerTasksListHandler(
      const userver::components::ComponentConfig& config,
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

    const auto& company_id = ctx.GetData<std::string>("company_id");
    const auto& user_id = ctx.GetData<std::string>("user_id");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        R"(
        SELECT
            t.title,
            t.project_id,
            t.task_id,
            t.creator,
            t.assignee
        FROM working_day_)" + company_id + R"(.tracker_tasks t
        WHERE t.creator = $1
          OR t.assignee = $1
          OR EXISTS (
              SELECT 1 FROM working_day_)" + company_id + R"(.tracker_task_observers o
              WHERE o.task_id = t.task_id AND o.employee_id = $1
          )
          OR EXISTS (
              SELECT 1 FROM working_day_)" + company_id + R"(.tracker_projects p
              WHERE p.project_id = t.project_id AND p.creator = $1
          )
          OR EXISTS (
              SELECT 1 FROM working_day_)" + company_id + R"(.tracker_project_assigned_users pau
              WHERE pau.project_id = t.project_id AND pau.employee_id = $1
          )
        ORDER BY t.created_ts DESC
        )",
        user_id
      );

    TrackerTasksListResponse response;
    response.tasks = result.AsContainer<std::vector<TrackerTasksItemResponseShort>>(
        userver::storages::postgres::kRowTag);

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerTasksList(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksListHandler>();
}

}  // namespace views::v1::tracker::tasks::list