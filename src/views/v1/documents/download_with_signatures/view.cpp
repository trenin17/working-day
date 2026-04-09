#define V1_DOCUMENTS_DOWNLOAD_WITH_SIGNATURES

#include "view.hpp"

#include <optional>
#include <string>

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
#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::documents::download_with_signatures {

namespace {

struct DocumentIdParentRow {
  std::string id;
  std::string parent_id;
};

std::string ResolveRootDocumentId(
    const userver::storages::postgres::ClusterPtr& pg,
    const std::string& company_id, const std::string& start_id) {
  std::string current = start_id;
  for (int guard = 0; guard < 64; ++guard) {
    auto r = pg->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, parent_id FROM working_day_" + company_id +
            ".documents WHERE id = $1",
        current);
    if (r.IsEmpty()) {
      return start_id;
    }
    auto row =
        r.AsSingleRow<DocumentIdParentRow>(userver::storages::postgres::kRowTag);
    if (row.parent_id == row.id) {
      return row.id;
    }
    current = row.parent_id;
  }
  return start_id;
}

std::optional<std::string> ResolveStampedDocumentId(
    const userver::storages::postgres::ClusterPtr& pg,
    const std::string& company_id, const std::string& requested_id,
    const std::string& root_id) {
  if (requested_id != root_id) {
    return requested_id;
  }
  auto child = pg->Execute(
      userver::storages::postgres::ClusterHostType::kSlave,
      "SELECT id FROM working_day_" + company_id +
          ".documents WHERE parent_id = $1 AND id <> parent_id "
          "ORDER BY created_ts DESC LIMIT 1",
      root_id);
  if (child.IsEmpty()) {
    return std::nullopt;
  }
  return child.AsSingleRow<std::string>();
}

// documents.name часто без .pdf; для Content-Disposition добавляем, если нет.
std::string EnsurePdfDownloadFilename(const std::string& name) {
  if (name.size() >= 4) {
    const auto ext = name.substr(name.size() - 4);
    if (ext == ".pdf" || ext == ".PDF") {
      return name;
    }
  }
  return name + ".pdf";
}

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
    is_testing:
        type: boolean
        description: Use mock S3 presigned links in tests
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
        pyservice_url_(config["pyservice-url"].As<std::string>()),
        is_testing_(config["is_testing"].As<bool>()) {}

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

    auto doc_check_author = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT 1 FROM working_day_" + company_id +
            ".documents d "
            "WHERE d.id = $1 AND d.author_id = $2",
        document_id, user_id);

    if (doc_check.IsEmpty() && doc_check_author.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Document not found or access denied"}.ToJsonString();
    }

    const std::string root_id =
        ResolveRootDocumentId(pg_cluster_, company_id, document_id);

    auto name_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT name FROM working_day_" + company_id +
            ".documents WHERE id = $1",
        root_id);
    if (name_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Document not found"}.ToJsonString();
    }
    const std::string root_name = name_result.AsSingleRow<std::string>();

    auto sig_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT signature_path, signature_type FROM working_day_" + company_id +
            ".document_signatures WHERE document_id = $1 ORDER BY created_ts ASC",
        root_id);

    if (sig_result.IsEmpty()) {
      const auto stamped_only_id = ResolveStampedDocumentId(
          pg_cluster_, company_id, document_id, root_id);
      if (stamped_only_id.has_value()) {
        json pep_body;
        pep_body["root_document_id"] = root_id;
        pep_body["original_display_name"] = root_name;
        pep_body["stamped_document_id"] = stamped_only_id.value();
        pep_body["pep_only_zip"] = true;
        pep_body["signatures"] = json::array();

        auto pep_resp = http_client_.CreateRequest()
                            .post(pyservice_url_)
                            .data(pep_body.dump())
                            .retry(1)
                            .timeout(std::chrono::milliseconds{60000})
                            .perform();
        pep_resp->raise_for_status();
        auto pep_json = json::parse(pep_resp->body());
        DownloadDocumentResponse response;
        response.url = pep_json["url"];
        return response.ToJsonString();
      }
      const std::string url =
          utils::s3_presigned_links::GenerateDocumentPresignedDownloadWithFilename(
              root_id, EnsurePdfDownloadFilename(root_name), is_testing_);
      DownloadDocumentResponse response;
      response.url = url;
      return response.ToJsonString();
    }

    json signatures = json::array();
    for (auto row : sig_result) {
      auto [signature_path, signature_type] =
          row.As<std::string, std::string>();
      json item;
      item["path"] = signature_path;
      item["type"] = signature_type;
      signatures.push_back(item);
    }

    const auto stamped_id = ResolveStampedDocumentId(
        pg_cluster_, company_id, document_id, root_id
    );

    json body;
    body["root_document_id"] = root_id;
    body["original_display_name"] = root_name;
    if (stamped_id.has_value()) {
      body["stamped_document_id"] = stamped_id.value();
    } else {
      body["stamped_document_id"] = nullptr;
    }
    body["signatures"] = signatures;

    auto resp = http_client_.CreateRequest()
                    .post(pyservice_url_)
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
  bool is_testing_{false};
};

}  // namespace

void AppendDocumentsDownloadWithSignatures(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsDownloadWithSignaturesHandler>();
}

}  // namespace views::v1::documents::download_with_signatures
