#define V1_TRACKER_TASKS_EDIT

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

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id +
            ".tracker_tasks "
            "SET title = case when $2 is null then title else $2 end, "
            "description = case when $3 is null then description else $3 end, "
            "project_name = case when $4 is null then project_name else $4 end, "
            "assignee = case when $5 is null then assignee else $5 end "
            "WHERE id = $1",
        task_id, request_body.title, request_body.description, request_body.project_name,
        request_body.assignee);

    return "";
  }
// сюда статус optional
 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerTasksEdit(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksEditHandler>();
}

}  // namespace views::v1::tracker::tasks::edit