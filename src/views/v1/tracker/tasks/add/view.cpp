#define V1_TRACKER_TASKS_ADD

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "definitions/all.hpp"

namespace views::v1::tracker::tasks::add {

namespace {

class TrackerTasksAddHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-add";

  TrackerTasksAddHandler(
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

    TrackerTasksAddRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());
    
    auto find_project = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT COUNT(*) FROM working_day_" + company_id +
            ".tracker_projects WHERE project_name = $1",
        request_body.project_name);

    int project_count = find_project.AsSingleRow<int>();
    if (!project_count) {
        request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"Wrong project name"}.ToJsonString();
    }

    auto find_tasks = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT tasks_count FROM working_day_" + company_id +
            ".tracker_projects WHERE project_name = $1",
        request_body.project_name);

    int project_tasks_count = find_tasks.AsSingleRow<int>();

    std::string id = request_body.project_name + "-" + std::to_string(project_tasks_count + 1);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".tracker_tasks (title, description, project_name, id) "
            "VALUES ($1, $2, $3, $4)",
        request_body.title,
        request_body.description,
        request_body.project_name,
        id);
    
    auto update_project = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id +
            ".tracker_projects SET tasks_count = tasks_count + 1 "
            "WHERE project_name = $1",
        request_body.project_name);

    return "Task added";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerTasksAdd(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksAddHandler>();
}

}  // namespace views::v1::tracker::tasks::add