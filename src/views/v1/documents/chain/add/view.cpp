#define V1_DOCUMENTS_CHAIN_ADD

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

namespace views::v1::documents::chain::add {

struct MetadataContainer {
    std::vector<DocumentsChainMetadataItemPg> chain_metadata;
};

namespace {

class DocumentsChainAddHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-chain-add";

  DocumentsChainAddHandler(
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

    DocumentsChainAddRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    if (request_body.chain_metadata.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Chain metadata cannot be empty"}.ToJsonString();
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT chain_metadata_new "
        "FROM working_day_" +
            company_id +
            ".documents "
            "WHERE id = $1",
        document_id);

    if (result.IsEmpty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"Document not found"}.ToJsonString();
    }

    auto existing_chain = result.AsSingleRow<MetadataContainer>(userver::storages::postgres::kRowTag).chain_metadata;
    if (!existing_chain.empty()) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
      return ErrorMessage{"Document already has a signature chain"}.ToJsonString();
    }

    std::vector<std::string> employee_ids;
    for (const auto& item : request_body.chain_metadata) {
      employee_ids.push_back(item.employee_id);
    }

    auto employees_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id FROM working_day_" + company_id + ".employees "
        "WHERE id = ANY($1)",
        employee_ids);

    if (employees_result.Size() != employee_ids.size()) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"One or more employee IDs are invalid"}.ToJsonString();
    }

    std::vector<DocumentsChainMetadataItemPg> chain_metadata_pg;
    for (const auto& item : request_body.chain_metadata) {
      chain_metadata_pg.emplace_back(
          DocumentsChainMetadataItemPg{
              item.employee_id,
              item.requires_signature,
              item.status
          });
    }

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id + ".documents "
        "SET chain_metadata_new = $2 "
        "WHERE id = $1 ",
        document_id,
        chain_metadata_pg);

    userver::storages::postgres::ParameterStore parameters;
    std::string filter;
    parameters.PushBack(document_id);

    for (const auto& item : chain_metadata_pg) {
        filter += "($" + std::to_string(parameters.Size() + 1) + ", $1),";
        parameters.PushBack(item.employee_id);
    }
    if (!filter.empty()) {
        filter.pop_back();
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO working_day_" + company_id + 
            ".employee_document(employee_id, document_id) "
            "VALUES " + filter + " ON CONFLICT DO NOTHING",
            parameters);
    }
    SendNotifications(company_id, document_id, user_id, chain_metadata_pg);
    return "Chain was added";
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
        notification_text = fmt::format("Документ '{}' был добавлен пользователем {}.", doc_name, employee_name);

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

void AppendDocumentsChainAdd(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsChainAddHandler>();
}

}  // namespace views::v1::documents::chain::add