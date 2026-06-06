#define V1_DOCUMENTS_LIST_ALL

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
#include <userver/yaml_config/merge_schemas.hpp>

#include <definitions/all.hpp>

#include "utils/s3_presigned_links.hpp"

namespace views::v1::documents::list_all {

namespace {

using HandlerBase = userver::server::handlers::HttpHandlerBase;

class DocumentsListAllHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-list-all";

  DocumentsListAllHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()),
        is_testing_(config["is_testing"].As<bool>()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    // const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT d.id, d.name, "
            "d.type, d.sign_required, "
            "d.description, NULL::BOOLEAN as signed, d.author_id, "
            "e.photo_link AS author_photo_url, "
            "NULL::TEXT as parent_id, d.created_ts, d.chain_metadata_new, d.visibility_status "
            "FROM working_day_" +
            company_id +
            ".documents d "
            "LEFT JOIN working_day_" +
            company_id +
            ".employees e ON e.id = d.author_id "
            "WHERE d.parent_id = d.id "
            "ORDER BY d.created_ts DESC, d.id ASC");

    DocumentsListAllResponse response;
    response.documents = result.AsContainer<std::vector<DocumentItem>>(
        userver::storages::postgres::kRowTag);

    for (auto& doc : response.documents) {
      if (doc.author_photo_url.has_value()) {
        doc.author_photo_url =
            utils::s3_presigned_links::GeneratePhotoPresignedLink(
                doc.author_photo_url.value(), utils::s3_presigned_links::Download,
                is_testing_);
      }
    }

    return response.ToJsonString();
  }

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Documents list-all handler
additionalProperties: false
properties:
    is_testing:
        type: boolean
        description: Use stub S3 presigned URLs in testsuite
)");
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  bool is_testing_ = false;
};

}  // namespace

void AppendDocumentsListAll(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsListAllHandler>();
}

}  // namespace views::v1::documents::list_all