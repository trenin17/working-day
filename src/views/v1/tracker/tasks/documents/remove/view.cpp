#define V1_TRACKER_TASKS_DOCUMENTS_REMOVE

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include <definitions/all.hpp>

namespace views::v1::tracker::tasks::documents::remove {

namespace {

class TrackerTasksDocumentsRemoveHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-documents-remove";

  TrackerTasksDocumentsRemoveHandler(
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

    auto task_id = request.GetArg("task_id");
    auto document_id = request.GetArg("document_id");

    if (task_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing task_id parameter"}.ToJsonString();
    }

    if (document_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing document_id parameter"}.ToJsonString();
    }

    const auto& company_id = ctx.GetData<std::string>("company_id");

    // Check if task exists
    auto task_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT project_id FROM working_day_" + company_id + ".tracker_tasks WHERE task_id = $1",
        task_id);

    if (task_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Task not found"}.ToJsonString();
    }

    std::string project_id = task_result.AsSingleRow<std::string>();

    // Check if document-task link exists
    auto link_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT 1 FROM working_day_" + company_id + ".task_documents "
        "WHERE task_id = $1 AND document_id = $2",
        task_id, document_id);

    if (link_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Document not found in task"}.ToJsonString();
    }

    // Remove link between task and document
    auto trx = pg_cluster_->Begin(
        "tracker_tasks_documents_remove",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    trx.Execute(
        "DELETE FROM working_day_" + company_id + ".task_documents "
        "WHERE task_id = $1 AND document_id = $2",
        task_id, document_id);

    // Update task timestamp
    trx.Execute(
        "UPDATE working_day_" + company_id + ".tracker_tasks "
        "SET last_updated_ts = NOW() WHERE task_id = $1",
        task_id);

    // Update project timestamp
    trx.Execute(
        "UPDATE working_day_" + company_id + ".tracker_projects "
        "SET last_updated_ts = NOW() WHERE project_id = $1",
        project_id);

    trx.Commit();

    return "{}";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerTasksDocumentsRemove(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksDocumentsRemoveHandler>();
}

}  // namespace views::v1::tracker::tasks::documents::remove