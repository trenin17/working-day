#define V1_TRACKER_TASKS_ASSIGNED_TO_USER

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "utils/s3_presigned_links.hpp"

#include "definitions/all.hpp"

namespace views::v1::tracker::tasks::assigned_to_user {

namespace {

class TrackerTasksAssignedToUserHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-assigned-to-user";

  TrackerTasksAssignedToUserHandler(
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

    const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");
    auto employee_id = request.GetArg("employee_id");

    if (employee_id.empty()) {
      employee_id = user_id;
    }

    LOG_INFO() << "employee_id: " << employee_id;

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT tasks.title, tasks.project_name, tasks.id, "
        "tasks.creator, tasks.assignee "
        "FROM working_day_" +
            company_id +
            ".tracker_tasks as tasks "
            "WHERE tasks.assignee = $1",
        employee_id);

    if (result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"No tasks found for the user"}.ToJsonString();
    }

    std::vector<TrackerTasksListItem> tasks;
    for (const auto& row : result) {
        tasks.emplace_back(row.As<TrackerTasksListItem>(userver::storages::postgres::kRowTag));
    }

    TrackerTasksListResponse response;
    response.tasks = tasks;

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerTasksAssignedToUser(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksAssignedToUserHandler>();
}

}  // namespace views::v1::tracker::tasks::assigned_to_user