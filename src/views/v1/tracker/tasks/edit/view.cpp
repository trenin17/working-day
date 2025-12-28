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

struct TaskValuesRow {
  std::optional<std::string> assignee;
  std::string title;
  std::string project_id;
  std::optional<std::string> action_id;
};

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
                           "ON CONFLICT (key, entity_type) DO UPDATE "
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
          "WHERE task_id = $1; ",
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
                                filter + ") "
                                "AND entity_type = 'tasks';",
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
      "SELECT assignee, title, project_id, action_id FROM working_day_" + company_id +
      ".tracker_tasks WHERE task_id = $1",
      task_id);

    if (check_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Task not found"}.ToJsonString();
    }
    auto task_value = check_result.AsSingleRow<TaskValuesRow>(userver::storages::postgres::kRowTag);

    TrackerTasksEditRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    core::reverse_index::TrackerTasksAllData data_new{task_id, request_body.title};
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

    // проверка, что наблюдатели валидны
    if (request_body.observers.has_value()) {
      auto check = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT id FROM working_day_" + company_id +
          ".employees WHERE id = ANY($1)",
          *request_body.observers);

      if (check.Size() != request_body.observers->size()) {
        request.GetHttpResponse().SetStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{
            "Invalid observers: one or more employees not found"}
            .ToJsonString();
      }
    }
    // проверка, что связанные задачи валидны
    if (request_body.related_tasks_ids) {
      auto check = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT task_id FROM working_day_" + company_id +
          ".tracker_tasks WHERE task_id = ANY($1)",
          *request_body.related_tasks_ids);

      if (check.Size() != request_body.related_tasks_ids->size()) {
        request.GetHttpResponse().SetStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{
            "Invalid related_tasks_ids: one or more tasks not found"}
            .ToJsonString();
      }
    }

    auto trx = pg_cluster_->Begin(
        "tracker_tasks_edit",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    // название проекта
    auto new_project_title = trx.Execute(
      "SELECT title FROM working_day_" + company_id +
      ".tracker_projects WHERE project_id = $1",
      request_body.project_id.value_or(task_value.project_id));

    if (new_project_title.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Project not found"}.ToJsonString();
    }
    std::string project_title = new_project_title.AsSingleRow<std::string>();

    trx.Execute(
      "UPDATE working_day_" + company_id +
        ".tracker_tasks "
        "SET title = case when $2 is null then title else $2 end, "
        "description = case when $3 is null then description else $3 end, "
        "project_id = case when $4 is null then project_id else $4 end, "
        "assignee = case when $5 is null then assignee else $5 end, "
        "status = case when $6 is null then status else $6 end, "
        "deadline = case when $7 is null then deadline else $7 end, "
        "priority = case when $8 is null then priority else $8 end, "
        "last_updated_ts = NOW() "
        "WHERE task_id = $1",
      task_id, request_body.title, request_body.description, request_body.project_id,
      request_body.assignee, request_body.status, request_body.deadline, request_body.priority);

    trx.Execute(
        "UPDATE working_day_" + company_id +".tracker_projects "
          "SET last_updated_ts = NOW() "
          "WHERE project_id = $1",
          request_body.project_id.value_or(task_value.project_id));

    // наблюдатели
    if (request_body.observers) {
        trx.Execute(
            "DELETE FROM working_day_" + company_id + ".tracker_task_observers "
            "WHERE task_id = $1",
            task_id
        );

        for (const auto& employee_id : *request_body.observers) {
            trx.Execute(
                "INSERT INTO working_day_" + company_id + ".tracker_task_observers "
                "(task_id, employee_id) "
                "VALUES ($1, $2) "
                "ON CONFLICT DO NOTHING",
                task_id, employee_id
            );
        }
    }

    // связанные задачи
    if (request_body.related_tasks_ids) {
      trx.Execute(
        "DELETE FROM working_day_" + company_id + ".tracker_task_related_tasks "
        "WHERE task_id = $1",
        task_id
      );

      for (const auto& task_id_related : *request_body.related_tasks_ids) {
        trx.Execute(
          "DELETE FROM working_day_" + company_id + ".tracker_task_related_tasks "
          "WHERE task_id = $1 and task_id_related = $2",
          task_id_related,
          task_id
        );
        trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".tracker_task_related_tasks (task_id, task_id_related) "
              "VALUES ($1, $2), ($2, $1)  ON CONFLICT DO NOTHING",
          task_id,
          task_id_related
        );
      }
    }

    // уведомления
    if (task_value.assignee.has_value() or request_body.assignee.has_value()) {
      auto assignee = request_body.assignee.value_or(task_value.assignee.value());

      std::string notification_text;
      if (assignee != task_value.assignee.value()) {
        notification_text =
          "Вам назначена новая задача \"" + request_body.title.value_or(task_value.title) +
          "\" в проекте \"" + project_title + "\".";

      if (task_value.action_id.has_value()) {
        trx.Execute(
          "UPDATE working_day_" + company_id +
            ".actions "
            "SET user_id = $1 "
            "WHERE id = $2",
          assignee, task_value.action_id.value());
      }
    } else {
      notification_text = "Изменена информация о задаче \"" + request_body.title.value_or(task_value.title) +
      "\" в проекте \"" + project_title + "\".";
    }


    auto notification_id = userver::utils::generators::GenerateUuid();
    trx.Execute(
      "INSERT INTO working_day_" + company_id +
          ".notifications(id, type, text, sender_id, user_id, task_id) "
          "VALUES ($1, $2, $3, $4, $5, $6) "
          "ON CONFLICT (id) DO NOTHING",
      notification_id, "generic", notification_text, user_id, assignee, task_id);

    // обновляем календарь

    // добавить ручку на документы

    // если раньше не было в календаре и добавили дедлайн
    if (!task_value.action_id.has_value() and request_body.deadline.has_value()) {
      auto action_id = userver::utils::generators::GenerateUuid();
      trx.Execute("INSERT INTO working_day_" + company_id +
                  ".actions(id, type, attendance_type, user_id, start_date, "
                  "end_date) "
                  "VALUES($1, $2, $3, $4, $5, $6) "
                  "ON CONFLICT (id) "
                  "DO NOTHING",
              action_id, "attendance", "tracker_task_deadline", assignee,
              request_body.deadline.value(), request_body.deadline.value());

      trx.Execute(
        "UPDATE working_day_" + company_id +
          ".tracker_tasks "
          "SET action_id = $1 "
          "WHERE task_id = $2",
        action_id, task_id);
    }
    // если обновили дедлайн
    else if (request_body.deadline.has_value()) {
      trx.Execute(
        "UPDATE working_day_" + company_id +
          ".actions "
          "SET start_date = $1, end_date = $1"
          "WHERE id = $2",
        request_body.deadline.value(), task_value.action_id.value());
      }
    }

    trx.Commit();
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