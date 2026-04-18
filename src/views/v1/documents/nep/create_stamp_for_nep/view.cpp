#define V1_DOCUMENTS_CREATE_STAMP_FOR_NEP

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

#include "definitions/all.hpp"

#include "views/v1/documents/nep/common/nep_common.hpp"

namespace nep_common = views::v1::documents::nep::common;

namespace views::v1::documents::nep::create_stamp_for_nep {

namespace {

class DocumentsCreateStampForNepHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName =
      "handler-v1-documents-create-stamp-for-nep";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Create visual NEP stamp on PDF (python service)
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

  DocumentsCreateStampForNepHandler(
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
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");
    const auto& document_id = request.GetArg("document_id");

    auto err = nep_common::RunCreateStampForNep(pg_cluster_, http_client_,
                                                  pyservice_url_, company_id,
                                                  user_id, document_id);
    if (err.has_value()) {
      request.GetHttpResponse().SetStatus(err->status);
      return err->body;
    }
    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsCreateStampForNep(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsCreateStampForNepHandler>();
}

}  // namespace views::v1::documents::nep::create_stamp_for_nep
