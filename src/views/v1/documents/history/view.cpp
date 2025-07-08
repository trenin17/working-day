#define V1_DOCUMENTS_HISTORY

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
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

namespace views::v1::documents::history {

namespace {

class DocumentsHistoryHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-history";

  DocumentsHistoryHandler(
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
    const auto& document_id = request.GetArg("document_id");

    if (document_id.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Missing document_id"}.ToJsonString();
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT actor_id, action_type, comment, created_ts "
        "FROM working_day_" + company_id + ".documents_history "
        "WHERE document_id = $1 ORDER BY created_ts",
        document_id);

    DocumentHistoryResponse response;
    response.history = result.AsContainer<std::vector<DocumentHistoryItem>>(
        userver::storages::postgres::kRowTag);

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendDocumentsHistory(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsHistoryHandler>();
}

}  // namespace views::v1::documents::history