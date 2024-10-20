#define V1_DOCUMENTS_UPLOAD

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>
#include "utils/s3_presigned_links.hpp"

namespace views::v1::documents::upload {

namespace {

class DocumentsUploadHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-upload";

  DocumentsUploadHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    UploadDocumentRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    const auto& user_id = ctx.GetData<std::string>("user_id");

    auto document_id =
        userver::utils::generators::GenerateUuid() + request_body.extension;
    auto upload_link = utils::s3_presigned_links::GenerateDocumentPresignedLink(
        document_id, utils::s3_presigned_links::Upload);

    UploadDocumentResponse response;
    response.id = document_id;
    response.url = upload_link;
    return response.ToJsonString();
  }
};

}  // namespace

void AppendDocumentsUpload(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsUploadHandler>();
}

}  // namespace views::v1::documents::upload