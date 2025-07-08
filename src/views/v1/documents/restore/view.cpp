#define V1_DOCUMENTS_RESTORE

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

namespace views::v1::documents::restore {

struct MetadataContainer {
    std::vector<DocumentsChainMetadataItemPg> chain_metadata;
};

namespace {

class DocumentsRestoreHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-restore";

  DocumentsRestoreHandler(
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

    DocumentsRemoveRestoreItem request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    if (request_body.document_id.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Missing document_id"}.ToJsonString();
    }

    auto perm_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT permission_value "
        "FROM working_day_" + 
            company_id +
            ".employee_permissions "
        "WHERE employee_id = $1 AND permission_type = 'can_remove_documents'",
        user_id);

    if (perm_result.IsEmpty() || perm_result.AsSingleRow<int>() == 0) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kForbidden);
        return ErrorMessage{"Insufficient rights"}.ToJsonString();
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT 1 "
        "FROM working_day_" + 
            company_id + 
            ".documents "
        "WHERE id = $1",
        request_body.document_id);

    if (result.IsEmpty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"Document not found"}.ToJsonString();
    }

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id + ".documents "
        "SET visibility_status = 0 "
        "WHERE id = $1",
        request_body.document_id);

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id + ".documents_history "
        "(document_id, actor_id, action_type, comment) "
        "VALUES ($1, $2, 'restored', $3)",
        request_body.document_id, user_id, request_body.comment);

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT chain_metadata_new "
        "FROM working_day_" +
            company_id +
            ".documents "
            "WHERE id = $1",
        request_body.document_id);
    auto chain_metadata_pg = result.AsSingleRow<MetadataContainer>(userver::storages::postgres::kRowTag).chain_metadata;
    
    SendNotifications(company_id, request_body.document_id, user_id, chain_metadata_pg);
    return "Document restored successfully";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;

  struct Employee {
    std::string name, surname;
  };

  void SendNotifications(
    const std::string& company_id,
    const std::string& document_id,
    const std::string& user_id,
    const std::vector<DocumentsChainMetadataItemPg>& chain_metadata) const {

        auto doc_result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT name FROM working_day_" + company_id + ".documents "
            "WHERE id = $1",
            document_id);

        std::string doc_name;
        if (!doc_result.IsEmpty()) {
            doc_name = doc_result.AsSingleRow<std::string>();
        }
    
        auto emp_result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT name, surname FROM working_day_" + company_id + ".employees "
            "WHERE id = $1",
            user_id);

        std::string employee_name;
        if (!emp_result.IsEmpty()) {
            auto emp_row = emp_result.AsSingleRow<Employee>(userver::storages::postgres::kRowTag);
            employee_name = emp_row.name + " " + emp_row.surname;
        }

        std::string notification_text;
        notification_text = fmt::format("Документ '{}' был восстановлен пользователем {}.", doc_name, employee_name);

        userver::storages::postgres::ParameterStore parameters;
        std::string filter;

        parameters.PushBack("generic");
        parameters.PushBack(notification_text);

        for (const auto& participant : chain_metadata) {
            auto notification_id = userver::utils::generators::GenerateUuid();

            filter += "($" + std::to_string(parameters.Size() + 1) + 
                    ", $1, $2, $" + 
                    std::to_string(parameters.Size() + 2) + "),";

            parameters.PushBack(notification_id);
            parameters.PushBack(participant.employee_id);
        }
        
        if (!filter.empty()) {
            filter.pop_back();

            pg_cluster_->Execute(
                userver::storages::postgres::ClusterHostType::kMaster,
                "INSERT INTO working_day_" + company_id + 
                ".notifications(id, type, text, user_id) "
                "VALUES " + filter + " ON CONFLICT (id) DO NOTHING",
                parameters);
        }
    }
};

}  // namespace

void AppendDocumentsRestore(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsRestoreHandler>();
}

}  // namespace views::v1::documents::restore