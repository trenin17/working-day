#define V1_DOCUMENTS_DOWNLOAD

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

#include <definitions/all.hpp>
#include "utils/s3_presigned_links.hpp"

namespace views::v1::documents::download {

namespace {

class DocumentsDownloadHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-download";

  DocumentsDownloadHandler(
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

    // TODO: check if this document is actually assigned to the requesting user
    // const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& document_id = request.GetArg("id");

    auto download_link =
        utils::s3_presigned_links::GenerateDocumentPresignedLink(
            document_id, utils::s3_presigned_links::Download);

    DownloadDocumentResponse response;
    response.url = download_link;
    return response.ToJsonString();
  }
};

}  // namespace

void AppendDocumentsDownload(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsDownloadHandler>();
}

}  // namespace views::v1::documents::download