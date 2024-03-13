#define V1_DOCUMENTS_SEND

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

namespace views::v1::documents::send {

namespace {

class DocumentsSendHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-send";

  DocumentsSendHandler(
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

    DocumentSendRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.documents(id, name, sign_required, description) "
        "VALUES($1, $2, $3, $4)",
        request_body.document.id, request_body.document.name, request_body.document.sign_required, request_body.document.description);

    LOG_INFO() << "BUILDING PARAMS";
    userver::storages::postgres::ParameterStore parameters;
    std::string filter;
    for (const auto& employee_id : request_body.employee_ids) {
        filter += "($" + std::to_string(parameters.Size() + 1) + ", $" + std::to_string(parameters.Size() + 2) + "),";
        parameters.PushBack(employee_id);
        LOG_INFO() << "PUSH BACK 1";
        parameters.PushBack(request_body.document.id);
        LOG_INFO() << "PUSH BACK 2";
    }
    filter.pop_back();
    LOG_INFO() << "QUERY " << filter;

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.employee_document "
        "(employee_id, document_id) "
        "VALUES " + filter,
        parameters
    );

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendDocumentsSend(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsSendHandler>();
}

}  // namespace views::v1::documents::send