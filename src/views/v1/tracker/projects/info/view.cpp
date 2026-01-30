#define V1_TRACKER_PROJECTS_INFO

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

namespace views::v1::tracker::projects::info {

namespace {

class InfoTrackerProjectsHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-projects-info";

  InfoTrackerProjectsHandler(
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
    auto project_id = request.GetArg("project_id");

    if (project_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing project_id parameter"}.ToJsonString();
    }

    auto result = pg_cluster_->Execute(
    userver::storages::postgres::ClusterHostType::kSlave,
    R"(
    SELECT
        p.project_id,
        p.title,
        p.description,
        p.image_url,
        p.creator,
        p.tasks_count,
        p.status,
        p.created_ts,
        p.last_updated_ts,
        COALESCE(
          array_agg(a.employee_id) FILTER (WHERE a.employee_id IS NOT NULL),
          '{}'
        ) AS assigned_users_ids
    FROM working_day_)" + company_id + R"(.tracker_projects p
    LEFT JOIN working_day_)" + company_id + R"(.tracker_project_assigned_users a
        ON a.project_id = p.project_id
    WHERE p.project_id = $1
    GROUP BY
        p.project_id,
        p.title,
        p.description,
        p.image_url,
        p.creator,
        p.tasks_count,
        p.status,
        p.created_ts,
        p.last_updated_ts
    )",
    project_id);

    if (result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Project not Found"}.ToJsonString();
    }

    TrackerProjectsItemResponse response{result.AsSingleRow<TrackerProjectsItemResponse>(userver::storages::postgres::kRowTag)};

    if (response.image_url.has_value()) {
        response.image_url = utils::s3_presigned_links::GenerateTrackerProjectsMediaPresignedLink(
            response.image_url.value(), utils::s3_presigned_links::Download, is_testing_);
    }

    return response.ToJsonString();
  }

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Tracker projects media upload handler schema
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

void AppendTrackerProjectsInfo(userver::components::ComponentList& component_list) {
  component_list.Append<InfoTrackerProjectsHandler>();
}

}  // namespace views::v1::tracker::projects::info