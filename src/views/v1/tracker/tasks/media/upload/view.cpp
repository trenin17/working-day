#define V1_TRACKER_TASKS_MEDIA_UPLOAD

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
#include <userver/yaml_config/merge_schemas.hpp>

#include "definitions/all.hpp"

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::tracker::tasks::media::upload {

namespace {

class TrackerTasksMediaUploadResponse {
 public:
  std::string ToJSON() {
    json j;
    j["url"] = url;

    return j.dump();
  }

  std::string url;
};

class TrackerTasksMediaUploadHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-media-upload";

  TrackerTasksMediaUploadHandler(
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


    auto task_id = request.GetArg("task_id");

    if (task_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing task_id parametr"}.ToJsonString();
    }
    
    const auto& company_id = ctx.GetData<std::string>("company_id");

    fprintf(stderr, "IS_TESTING: %d\n", is_testing_);


    auto media_id = userver::utils::generators::GenerateUuid();
    auto upload_link = utils::s3_presigned_links::GenerateTrackerTasksMediaPresignedLink(
        media_id, utils::s3_presigned_links::Upload, is_testing_);


    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id +
            ".tracker_tasks "
            "SET media_links = array_append(media_links, $2) "
            "WHERE id = $1",
        task_id, media_id);

    TrackerTasksMediaUploadResponse response{upload_link};
    return response.ToJSON();
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

void AppendTrackerTasksMediaUpload(
    userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksMediaUploadHandler>();
}

}  // namespace views::v1::tracker::tasks::media::upload
