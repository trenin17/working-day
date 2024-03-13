#define V1_DOCUMENTS_UPLOAD

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "utils/s3_presigned_links.hpp"
#include <definitions/all.hpp>

namespace views::v1::documents::upload {

namespace {

class DocumentsUploadHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-upload";

  DocumentsUploadHandler(
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

    auto document_id = userver::utils::generators::GenerateUuid();
    auto upload_link = utils::s3_presigned_links::GeneratePhotoPresignedLink(
        document_id, utils::s3_presigned_links::Upload);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day.employees "
        "SET photo_link = $2 "
        "WHERE id = $1",
        user_id, document_id);

    UploadDocumentResponse response;
    response.id = document_id;
    response.url = upload_link;
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendDocumentsUpload(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsUploadHandler>();
}

}  // namespace views::v1::documents::upload