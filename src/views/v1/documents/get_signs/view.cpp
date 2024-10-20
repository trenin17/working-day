#define V1_DOCUMENTS_GET_SIGNS

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

namespace views::v1::documents::get_signs {

namespace {

class DocumentsGetSignsHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-get-signs";

  DocumentsGetSignsHandler(
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

    // const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");
    const auto& document_id = request.GetArg("document_id");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT ROW "
        "(e.id, e.name, e.surname, e.patronymic, "
        "e.photo_link), ed.signed "
        "FROM working_day_" +
            company_id +
            ".employees e "
            "JOIN working_day_" +
            company_id +
            ".employee_document ed ON e.id = "
            "ed.employee_id "
            "JOIN working_day_" +
            company_id +
            ".documents d ON ed.document_id = d.id "
            "WHERE d.parent_id = $1",
        document_id);

    DocumentsGetSignsResponse response;
    response.signs = result.AsContainer<std::vector<SignItem>>(
        userver::storages::postgres::kRowTag);

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendDocumentsGetSigns(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsGetSignsHandler>();
}

}  // namespace views::v1::documents::get_signs