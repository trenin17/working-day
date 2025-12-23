#define V1_TRACKER_TASKS_ADD
#define USERVER_POSTGRES_ENABLE_LEGACY_TIMESTAMP 1


#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>


#include "definitions/all.hpp"

#include "core/reverse_index/view.hpp"

namespace views::v1::tracker::tasks::add {

namespace {

struct ProjectValuesRow {
  int tasks_count;
  std::string title;
};

struct Employee {
  std::string surname;
  std::string name;
};

struct Notification {
  std::string text;
  std::string user_id;
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
                           "] AS keys, $1 AS task_id "
                           ") "
                           "INSERT INTO working_day_" +
                           data.company_id +
                           ".reverse_index (key, ids, entity_type) "
                           "SELECT key, ARRAY[task_id] AS ids, 'tasks' AS entity_type "
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

    TrackerTasksItemRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto find_project = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT COUNT(*) FROM working_day_" + company_id +
            ".tracker_projects WHERE project_id = $1",
        request_body.project_id);

    int project_count = find_project.AsSingleRow<int>();
    if (!project_count) {
        request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"Wrong project task_id"}.ToJsonString();
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
        "SELECT tasks_count, title FROM working_day_" + company_id +
            ".tracker_projects WHERE project_id = $1",
        request_body.project_id);

    ProjectValuesRow project_values = find_tasks.AsSingleRow<ProjectValuesRow>(userver::storages::postgres::kRowTag);
    std::string task_id = request_body.project_id + "-" + std::to_string(project_values.tasks_count + 1);

    if (request_body.observers) {
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
        "tracker_tasks_add",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    trx.Execute(
        "INSERT INTO working_day_" + company_id + ".tracker_tasks "
        "(task_id, title, project_id, description, assignee, "
        "deadline, status, priority, creator) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
        task_id,
        request_body.title,
        request_body.project_id,
        request_body.description,
        request_body.assignee,
        request_body.deadline,
        request_body.status.value_or("Open"),
        request_body.priority.value_or("Low"),
        user_id);

    if (request_body.observers) {
      for (const auto& employee_id : *request_body.observers) {
        trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".tracker_task_observers (task_id, employee_id) "
              "VALUES ($1, $2)  ON CONFLICT DO NOTHING",
          task_id,
          employee_id
        );
      }
    }
    if (request_body.related_tasks_ids) {
      for (const auto& task_id_related : *request_body.related_tasks_ids) {
        trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".tracker_task_related_tasks (task_id, task_id_related) "
              "VALUES ($1, $2), ($2, $1)  ON CONFLICT DO NOTHING",
          task_id,
          task_id_related
        );
      }
    }
    core::reverse_index::TrackerTasksAllData data{task_id, request_body.title};
    data.company_id = company_id;

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data]() -> core::reverse_index::ReverseIndexResponse {
          return AddTaskToReverseIndexFunc(cluster, data);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    trx.Execute(
        "UPDATE working_day_" + company_id +
            ".tracker_projects SET tasks_count = tasks_count + 1, last_updated_ts = NOW() "
            "WHERE project_id = $1",
        request_body.project_id);

    std::vector<Notification> notifications;

    std::unordered_set<std::string> recipients;
    if (request_body.assignee.has_value() && *request_body.assignee != user_id) {
      auto notification_text =
          "Вам назначена новая задача \"" + request_body.title +
          "\" в проекте \"" + project_values.title + "\".";

      notifications.emplace_back(notification_text, *request_body.assignee);
    }

    if (request_body.observers) {
      std::string notification_text;
      if (request_body.assignee.has_value()) {
        auto assignee_name = trx.Execute(
            "SELECT surname, name FROM working_day_" + company_id +
            ".employees WHERE id = $1",
            *request_body.assignee);

        if (assignee_name.IsEmpty()) {
          request.GetHttpResponse().SetStatus(
              userver::server::http::HttpStatus::kNotFound);
          return ErrorMessage{"Employee not found"}.ToJsonString();
        }

        const auto employee = assignee_name.AsSingleRow<Employee>(userver::storages::postgres::kRowTag);

        notification_text =
            "Вам доступна к наблюдению новая задача \"" + request_body.title +
            "\" в проекте \"" + project_values.title + "\", назначенная пользователю \"" +
            employee.surname + " " + employee.name + "\".";
      } else {
        notification_text = "Вам доступна к наблюдению новая задача \"" + request_body.title +
            "\" в проекте \"" + project_values.title + "\".";
      }

      for (const auto& observer : *request_body.observers) {
        if (observer != user_id) {
          notifications.emplace_back(notification_text, observer);
        }
      }
    }

    if (!notifications.empty()) {
      userver::storages::postgres::ParameterStore params;
      std::string values;

      params.PushBack("generic");
      params.PushBack(user_id);
      params.PushBack(task_id);

      for (const auto& notification : notifications) {
        auto notification_id = userver::utils::generators::GenerateUuid();

        values +=
            "($" + std::to_string(params.Size() + 1) + ", " // id
            "$1, $" + std::to_string(params.Size() + 2) + ", " // text
            "$2, $" + std::to_string(params.Size() + 3) + ", $3),"; // sender, user

        params.PushBack(notification_id);
        params.PushBack(notification.text);
        params.PushBack(notification.user_id);
      }

      values.pop_back();

      trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".notifications(id, type, text, sender_id, user_id, task_id) "
              "VALUES " + values +
              " ON CONFLICT (id) DO NOTHING",
          params);
    }

    if (request_body.deadline.has_value() and request_body.assignee.has_value()) {
      auto action_id = userver::utils::generators::GenerateUuid();
      trx.Execute("INSERT INTO working_day_" + company_id +
                  ".actions(id, type, attendance_type, user_id, start_date, "
                  "end_date) "
                  "VALUES($1, $2, $3, $4, $5, $6) "
                  "ON CONFLICT (id) "
                  "DO NOTHING",
              action_id, "attendance", "tracker_task_deadline", request_body.assignee.value(),
              request_body.deadline.value(), request_body.deadline.value());

      trx.Execute(
        "UPDATE working_day_" + company_id +
          ".tracker_tasks "
          "SET action_id = $1 "
          "WHERE task_id = $2",
        action_id, task_id);
    }

    trx.Commit();

    TrackerTasksAddResponse response;
    response.task_id = task_id;
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerTasksAdd(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksAddHandler>();
}

}  // namespace views::v1::tracker::tasks::add