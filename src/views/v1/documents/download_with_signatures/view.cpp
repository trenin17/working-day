#define V1_DOCUMENTS_DOWNLOAD_WITH_SIGNATURES

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

using json = nlohmann::json;

namespace views::v1::documents::download_with_signatures {

namespace {

class DocumentsDownloadWithSignaturesHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName =
      "handler-v1-documents-download-with-signatures";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Download document with all signatures as a single archive
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Base URL of Python service (e.g. http://python-service:3000)
)");
  }

  DocumentsDownloadWithSignaturesHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(component_context
                        .FindComponent<userver::components::Postgres>(
                            "key-value")
                        .GetCluster()),
        http_client_(
            component_context.FindComponent<userver::components::HttpClient>()
                .GetHttpClient()),
        pyservice_url_(config["pyservice-url"].As<std::string>()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& company_id = ctx.GetData<std::string>("company_id");
    const auto& document_id = request.GetArg("id");
    if (document_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing document id"}.ToJsonString();
    }

    const auto& user_id = ctx.GetData<std::string>("user_id");
    auto doc_check = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT 1 FROM working_day_" + company_id +
            ".documents d "
            "JOIN working_day_" +
            company_id +
            ".employee_document ed ON d.id = ed.document_id "
            "WHERE d.id = $1 AND ed.employee_id = $2",
        document_id, user_id);
    if (doc_check.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Document not found or access denied"}.ToJsonString();
    }

    auto sig_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT signature_path FROM working_day_" + company_id +
            ".document_signatures WHERE document_id = $1",
        document_id);

    std::vector<std::string> signature_paths;
    for (auto row : sig_result) {
      signature_paths.push_back(row.As<std::string>());
    }

    json body;
    body["document_id"] = document_id;
    body["signature_paths"] = signature_paths;

    std::string url_to_call = pyservice_url_ + "/document/build-archive";
    auto resp = http_client_.CreateRequest()
                    .post(url_to_call)
                    .data(body.dump())
                    .retry(1)
                    .timeout(std::chrono::milliseconds{60000})
                    .perform();

    resp->raise_for_status();
    auto response_json = json::parse(resp->body());
    std::string archive_url = response_json["url"];

    DownloadDocumentResponse response;
    response.url = archive_url;
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsDownloadWithSignatures(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsDownloadWithSignaturesHandler>();
}

}  // namespace views::v1::documents::download_with_signatures
