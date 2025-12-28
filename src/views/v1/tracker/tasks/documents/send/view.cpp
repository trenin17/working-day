#define V1_TRACKER_TASKS_DOCUMENTS_SEND

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include <definitions/all.hpp>

namespace views::v1::tracker::tasks::documents::send {

namespace {

class TrackerTasksDocumentsSendHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-tasks-documents-send";

  TrackerTasksDocumentsSendHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()),
        http_client_(
            component_context.FindComponent<userver::components::HttpClient>()
                .GetHttpClient()),
        pyservice_url_(config["pyservice-url"].As<std::string>()) {}

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
      return ErrorMessage{"Missing task_id parameter"}.ToJsonString();
    }

    const auto& company_id = ctx.GetData<std::string>("company_id");

    TrackerTasksDocumentItem request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

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

    if (request_body.document_id.ends_with(".docx")) {
        PyserviceDocumentSendRequest py_request;
        py_request.file_key = request_body.document_id;

        request_body.document_id =
            request_body.document_id.substr(0, request_body.document_id.size() - std::string(".docx").size()) + ".pdf";
        py_request.converted_file_key = request_body.document_id;


        auto response = http_client_.CreateRequest()
                            .post(pyservice_url_)
                            .data(py_request.ToJsonString())
                            .retry(2)  // retry once in case of error
                            .timeout(std::chrono::milliseconds{10000})
                            .perform();  // start performing the request
        response->raise_for_status();
    }

    // Insert document record with visibility_status = 1 (archived) if it doesn't exist
    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id + ".documents(id, name, description, sign_required, visibility_status, created_ts) "
        "VALUES($1, $2, $3, $4, 1, NOW()) ON CONFLICT (id) DO NOTHING",
        request_body.document_id,
        request_body.name,
        request_body.description.value_or(""),
        false);

    // Create link between task and document
    auto trx = pg_cluster_->Begin(
        "tracker_tasks_documents_send",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    trx.Execute(
        "INSERT INTO working_day_" + company_id + ".task_documents(task_id, document_id) "
        "VALUES($1, $2) ON CONFLICT DO NOTHING",
        task_id, request_body.document_id);

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

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Task document send handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendTrackerTasksDocumentsSend(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerTasksDocumentsSendHandler>();
}

}  // namespace views::v1::tracker::tasks::documents::send