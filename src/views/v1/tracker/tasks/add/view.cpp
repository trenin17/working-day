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

#include "core/reverse_index/view.hpp"

namespace views::v1::tracker::tasks::add {

namespace {

core::reverse_index::ReverseIndexResponse AddTaskToReverseIndexFunc(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::TrackerTasksAllData data) {

    std::vector<std::string> words;
    std::istringstream stream(data.title.value());
    std::string word;

    while (stream >> word) {
        words.push_back(core::reverse_index::ConvertToLower(word));
    }

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(data.task_id);

  for (auto& w : words) {
    auto separator = (parameters.Size() == 1 ? "[" : ", ");
    parameters.PushBack(w);
    filter += fmt::format("{}${}", separator, parameters.Size());
  }

  auto result =
      cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "WITH input_data AS ( "
                       "  SELECT ARRAY" +
                           filter +
                           "] AS keys, $1 AS id "
                           ") "
                           "INSERT INTO working_day_" +
                           data.company_id +
                           ".reverse_index (key, ids, entity_type) "
                           "SELECT key, ARRAY[id] AS ids, 'tasks' AS entity_type "
                           "FROM input_data, LATERAL unnest(keys) AS key "
                           "ON CONFLICT (key) DO UPDATE "
                           "SET ids = array_append(working_day_" +
                           data.company_id +
                           ".reverse_index.ids, "
                           "EXCLUDED.ids[1]); ",
                       parameters);

  core::reverse_index::ReverseIndexResponse response(data.task_id);

  return response;
}
    

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

    const auto& user_id = ctx.GetData<std::string>("user_id");
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
    if (request_body.assignee.has_value()) {
        auto find_employee = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "SELECT COUNT(*) FROM working_day_" + company_id +
                ".employees WHERE id = $1",
            request_body.assignee.value());
        
        int employess_count = find_employee.AsSingleRow<int>();
        if (!employess_count) {
            request.GetHttpResponse().SetStatus(
                userver::server::http::HttpStatus::kNotFound);
            return ErrorMessage{"Wrong assignee"}.ToJsonString();
        }
    
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
            ".tracker_tasks (title, description, project_name, id, creator, assignee, status) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7)",
        request_body.title,
        request_body.description,
        request_body.project_name,
        id,
        user_id,
        request_body.assignee,
        "Open");

    core::reverse_index::TrackerTasksAllData data{id, request_body.title};
    data.company_id = company_id;

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data]() -> core::reverse_index::ReverseIndexResponse {
          return AddTaskToReverseIndexFunc(cluster, data);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);
    
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