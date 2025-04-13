#define V1_TRACKER_TASKS_EDIT
#define USERVER_POSTGRES_ENABLE_LEGACY_TIMESTAMP 1

#include "view.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/http/http.h>
#include <aws/s3/S3Client.h>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "core/json_compatible/struct.hpp"
#include "core/reverse_index/view.hpp"

#include "definitions/all.hpp"

namespace views::v1::tracker::tasks::edit {

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

struct EditTaskValuesRow {
  std::optional<std::string> title;
};

core::reverse_index::TrackerTasksAllData FetchOldTaskData(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::TrackerTasksAllData data) {
  auto grab_result = cluster->Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      "SELECT CASE WHEN $2 IS NULL THEN NULL ELSE title END "
      "FROM working_day_" +
          data.company_id +
          ".tracker_tasks "
          "WHERE id = $1; ",
      data.task_id, data.title);

  auto old_values = grab_result.AsSingleRow<EditTaskValuesRow>(
      userver::storages::postgres::kRowTag);

  core::reverse_index::TrackerTasksAllData data_old{data.task_id, old_values.title};
  data_old.company_id = data.company_id;

  return data_old;
}

core::reverse_index::ReverseIndexResponse EditTaskReverseIndexFunc(
  userver::storages::postgres::ClusterPtr cluster,
  core::reverse_index::TrackerTasksAllData old_data,
  core::reverse_index::TrackerTasksAllData new_data) {

  if (old_data.title && new_data.title &&
      old_data.title.value() != new_data.title.value()) {

      userver::storages::postgres::ParameterStore parameters;
      parameters.PushBack(old_data.task_id);

      std::vector<std::string> old_words;
      std::istringstream old_stream(old_data.title.value());
      std::string word;

      while (old_stream >> word) {
        old_words.push_back(core::reverse_index::ConvertToLower(word));
      }

      std::string filter;
      for (const auto& w : old_words) {
        auto separator = (parameters.Size() == 1 ? "(" : ", ");
        parameters.PushBack(w);
        filter += fmt::format("{}${}", separator, parameters.Size());
      }
      if (parameters.Size() > 1) {
        auto result = 
          cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                            "UPDATE working_day_" + old_data.company_id +
                                ".reverse_index "
                                "SET ids = array_remove(ids, $1) "
                                "WHERE key IN " +
                                filter + ");",
                            parameters);
      }
      return AddTaskToReverseIndexFunc(cluster, new_data);
  }
  core::reverse_index::ReverseIndexResponse response(new_data.task_id);
  return response;
}

class TrackerTasksEditHandler : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-edit";

  TrackerTasksEditHandler(
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

    auto task_id = request.GetArg("task_id");

    if (task_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing task_id parametr"}.ToJsonString();
    }

    auto check_result = pg_cluster_->Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      "SELECT 1 FROM working_day_" + company_id + ".tracker_tasks WHERE id = $1",
      task_id);

    if (check_result.IsEmpty()) {
        request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"Task not found"}.ToJsonString();
    }

    TrackerTasksEditRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    core::reverse_index::TrackerTasksAllData data_new{task_id,
                                                      request_body.title};
    data_new.company_id = company_id;

    core::reverse_index::TrackerTasksAllData data_old =
        FetchOldTaskData(pg_cluster_, data_new);

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data_old,
         data_new]() -> core::reverse_index::ReverseIndexResponse {
          return EditTaskReverseIndexFunc(cluster, data_old, data_new);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id +
            ".tracker_tasks "
            "SET title = case when $2 is null then title else $2 end, "
            "description = case when $3 is null then description else $3 end, "
            "project_name = case when $4 is null then project_name else $4 end, "
            "assignee = case when $5 is null then assignee else $5 end, "
            "status = case when $6 is null then status else $6 end, "
            "deadline = case when $7 is null then deadline else $7 end "
            "WHERE id = $1",
        task_id, request_body.title, request_body.description, request_body.project_name,
        request_body.assignee, request_body.status, request_body.deadline);

    return "Task was changed";
  }

private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerTasksEdit(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksEditHandler>();
}

}  // namespace views::v1::tracker::tasks::edit