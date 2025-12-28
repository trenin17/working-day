#define V1_TRACKER_TASKS_INFO

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "utils/s3_presigned_links.hpp"

#include "definitions/all.hpp"

namespace views::v1::tracker::tasks::info {

namespace {

class InfoTrackerTasksHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-info";

  InfoTrackerTasksHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()),
        is_testing_(config["is_testing"].As<bool>()) {}

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
      return ErrorMessage{"Missing task_id parameter"}.ToJsonString();
    }

    auto result = pg_cluster_->Execute(
    userver::storages::postgres::ClusterHostType::kSlave,
    R"(
    SELECT
        t.task_id,
        t.title,
        t.project_id,
        t.description,
        t.creator,
        t.assignee,
        t.status,
        t.priority,
        t.media_links,
        t.created_ts,
        t.last_updated_ts,
        t.deadline,
        t.action_id,
        COALESCE(obs.observers, '{}') AS observers,
        COALESCE(rel.related_tasks_ids, '{}') AS related_tasks_ids,
        COALESCE(docs.document_ids, '{}') AS document_ids
    FROM working_day_)" + company_id + R"(.tracker_tasks t
    LEFT JOIN (
        SELECT task_id, array_agg(employee_id) AS observers
        FROM working_day_)" + company_id + R"(.tracker_task_observers
        GROUP BY task_id
    ) obs ON obs.task_id = t.task_id
    LEFT JOIN (
        SELECT task_id, array_agg(task_id_related) AS related_tasks_ids
        FROM working_day_)" + company_id + R"(.tracker_task_related_tasks
        GROUP BY task_id
    ) rel ON rel.task_id = t.task_id
    LEFT JOIN (
        SELECT task_id, array_agg(document_id) AS document_ids
        FROM working_day_)" + company_id + R"(.task_documents
        GROUP BY task_id
    ) docs ON docs.task_id = t.task_id
    WHERE t.task_id = $1
    )",
    task_id);


    if (result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Task not Found"}.ToJsonString();
    }

    TrackerTasksItemResponse response{result.AsSingleRow<TrackerTasksItemResponse>(userver::storages::postgres::kRowTag)};

    if (response.media_links.has_value()) {
      for (auto& link : response.media_links.value()) {
          link = utils::s3_presigned_links::GenerateTrackerTasksMediaPresignedLink(
              link, utils::s3_presigned_links::Download, is_testing_);
      }
    }

    return response.ToJsonString();
  }

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Tracker tasks media upload handler schema
additionalProperties: false
properties:
    is_testing:
        type: boolean
        description: flag for testing mode
)");
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  bool is_testing_ = false;
};

}  // namespace

void AppendTrackerTasksInfo(userver::components::ComponentList& component_list) {
  component_list.Append<InfoTrackerTasksHandler>();
}

}  // namespace views::v1::tracker::tasks::info