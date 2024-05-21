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
        "(working_day_" + company_id + ".employees.id, working_day_" + company_id + ".employees.name, "
        "working_day_" + company_id + ".employees.surname, working_day_" + company_id + ".employees.patronymic, "
        "working_day_" + company_id + ".employees.photo_link), "
        "working_day_" + company_id + ".employee_document.signed "
        "FROM working_day_" + company_id + ".employees "
        "JOIN working_day_" + company_id + ".employee_document ON working_day_" + company_id + ".employees.id = "
        "working_day_" + company_id + ".employee_document.employee_id "
        "WHERE working_day_" + company_id + ".employee_document.document_id = $1",
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