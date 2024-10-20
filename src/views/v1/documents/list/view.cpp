#define V1_DOCUMENTS_LIST

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

#include <definitions/all.hpp>

namespace views::v1::documents::list {

namespace {

class DocumentsListHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-list";

  DocumentsListHandler(
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
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT working_day_" + company_id + ".documents.id, working_day_" +
            company_id +
            ".documents.name, "
            "working_day_" +
            company_id + ".documents.type, working_day_" + company_id +
            ".documents.sign_required, "
            "working_day_" +
            company_id +
            ".documents.description, "
            "working_day_" +
            company_id +
            ".employee_document.signed, "
            "NULL::TEXT as parent_id "
            "FROM working_day_" +
            company_id +
            ".documents "
            "JOIN working_day_" +
            company_id + ".employee_document ON working_day_" + company_id +
            ".documents.id = "
            "working_day_" +
            company_id +
            ".employee_document.document_id "
            "WHERE working_day_" +
            company_id +
            ".employee_document.employee_id = $1 "
            "ORDER BY working_day_" +
            company_id + ".documents.created_ts DESC",
        user_id);

    DocumentsListResponse response;
    response.documents = result.AsContainer<std::vector<DocumentItem>>(
        userver::storages::postgres::kRowTag);

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendDocumentsList(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsListHandler>();
}

}  // namespace views::v1::documents::list