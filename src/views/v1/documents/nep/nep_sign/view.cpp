#define V1_DOCUMENTS_NEP_SIGN

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

namespace views::v1::documents::nep::nep_sign {

namespace {

class DocumentsNepSignHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-nep-sign";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Documents NEP sign handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service (document/nep-sign)
    pyservice-create-stamp-url:
        type: string
        description: Url of python service (document/create-stamp-for-nep), after NEP if sign_required != 0
)");
  }

  DocumentsNepSignHandler(
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
        pyservice_nep_sign_url_(config["pyservice-url"].As<std::string>()),
        pyservice_create_stamp_url_(
            config["pyservice-create-stamp-url"].As<std::string>()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto document_id = request.GetArg("document_id");
    auto signature_password = request.GetArg("signature_password");

    if (signature_password.empty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"signature_password is required"}.ToJsonString();
    }

    LOG_INFO() << "Signing document with NEP: " << document_id
               << " by user: " << user_id;

    DocumentsNepSignRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    nep_common::NepSignResult nep_out;
    auto err = nep_common::RunNepSign(
        pg_cluster_, http_client_, pyservice_nep_sign_url_, company_id, user_id,
        document_id, signature_password, request_body.reason, request_body.location,
        nep_out);
    if (err.has_value()) {
      request.GetHttpResponse().SetStatus(err->status);
      return err->body;
    }

    auto stamp_err = nep_common::TryCreateStampAfterNepSign(
        pg_cluster_, http_client_, pyservice_create_stamp_url_, company_id,
        user_id, document_id);
    if (stamp_err.has_value()) {
      LOG_ERROR() << "NEP sign succeeded but create-stamp failed for document_id="
                  << document_id << " user_id=" << user_id;
      request.GetHttpResponse().SetStatus(stamp_err->status);
      return stamp_err->body;
    }

    DocumentsNepSignResponse result;
    result.signature_id = std::move(nep_out.signature_id);
    result.signature_path = std::move(nep_out.signature_path);
    result.timestamp = std::move(nep_out.timestamp);
    result.public_key_hash = std::move(nep_out.public_key_hash);

    LOG_INFO() << "Successfully signed document: " << document_id
               << " with signature: " << result.signature_id;

    return result.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_nep_sign_url_;
  std::string pyservice_create_stamp_url_;
};

}  // namespace

void AppendDocumentsNepSign(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsNepSignHandler>();
}

}  // namespace views::v1::documents::nep::nep_sign
