#define V1_DOCUMENTS_CHAIN_UPDATE

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

namespace views::v1::documents::chain::update {

struct MetadataContainer {
    std::vector<DocumentsChainMetadataItemPg> chain_metadata;
};

namespace {

class DocumentsChainUpdateHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-chain-update";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Abscence verdict handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

  DocumentsChainUpdateHandler(
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
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");
    const auto& document_id = request.GetArg("document_id");

    DocumentsChainUpdateRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT chain_metadata "
        "FROM working_day_" +
            company_id +
            ".documents "
            "WHERE id = $1",
        document_id);

    if (result.IsEmpty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"Document not found"}.ToJsonString();
    }

    auto chain_metadata =
        result.AsSingleRow<MetadataContainer>(userver::storages::postgres::kRowTag).chain_metadata;

    if (!chain_metadata.empty()) {
        auto rejected_it = std::find_if(chain_metadata.begin(), chain_metadata.end(),
        [](const auto& item) { return item.status == 2; });
        if (rejected_it != chain_metadata.end()) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
            return ErrorMessage{"Document already rejected"}.ToJsonString();
        }
    }

    auto current_it = std::find_if(chain_metadata.begin(), chain_metadata.end(),
        [](const auto& item) { 
            return item.status == 0; 
        });

    bool is_first_signature = !std::any_of(chain_metadata.begin(), current_it,
        [](const auto& item) { 
            return item.requires_signature && item.status == 1; 
        });

    if (current_it == chain_metadata.end()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
        return ErrorMessage{"No pending approval steps"}.ToJsonString();
    }

    if (current_it->employee_id != user_id) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Not your turn to approve"}.ToJsonString();
    }

    if (request_body.approval_status == 2) {
        current_it->status = 2;
        SendNotifications(company_id, document_id, user_id, chain_metadata, "rejected");
    } else if (request_body.approval_status == current_it->requires_signature) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        if (current_it->requires_signature)
            return ErrorMessage{"Document requires a signature"}.ToJsonString();  
        else
            return ErrorMessage{"Document doesn't require a signature"}.ToJsonString();  
    } else if (!current_it->requires_signature) {
        current_it->status = 1;
        SendNotifications(company_id, document_id, user_id, chain_metadata, "approved");
    } else {
        result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT id, name, surname, patronymic, photo_link, subcompany "
            "FROM working_day_" +
                company_id +
                ".employees "
                "WHERE id = $1",
            user_id);
        auto employee_info = result.AsSingleRow<ListEmployeeWithSubcompany>(
            userver::storages::postgres::kRowTag);
    
        PyserviceDocumentSignRequest py_request;
        py_request.employee_id = user_id;
        py_request.employee_name = employee_info.name;
        py_request.employee_surname = employee_info.surname;
        py_request.employee_patronymic = employee_info.patronymic;
        py_request.subcompany = employee_info.subcompany;
        py_request.file_key = document_id;
        py_request.signed_file_key = document_id;
        py_request.is_first_signature = is_first_signature;
    
        auto response = http_client_.CreateRequest()
                            .post(pyservice_url_)
                            .data(py_request.ToJsonString())
                            .retry(2)  // retry once in case of error
                            .timeout(std::chrono::milliseconds{5000})
                            .perform();  // start performing the request
        response->raise_for_status();
    
        result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "UPDATE working_day_" + company_id + ".employee_document "
            "SET signed = true "
            "WHERE employee_id = $1 AND document_id = $2",
            user_id, document_id);

        result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "UPDATE working_day_" + company_id + ".documents "
            "SET sign_required = false "
            "WHERE id = $1",
            document_id);
        current_it->status = 1;
        SendNotifications(company_id, document_id, user_id, chain_metadata, "signed and approved");
    }

    size_t element_index = current_it - chain_metadata.begin();
    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id + ".documents "
        "SET chain_metadata[" + std::to_string(element_index + 1) + "] = $2 "
        "WHERE id = $1",
        document_id,
        DocumentsChainMetadataItemPg{
            current_it->employee_id,
            current_it->requires_signature,
            current_it->status
        }
    );
    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT chain_metadata "
        "FROM working_day_" +
            company_id +
            ".documents "
            "WHERE id = $1",
        document_id);

    DocumentsChainUpdateResponse response = result.AsSingleRow<DocumentsChainUpdateResponse>(userver::storages::postgres::kRowTag);
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;

  struct Employee {
    std::string name, surname;
  };

  void SendNotifications(
    const std::string& company_id,
    const std::string& document_id,
    const std::string& user_id,
    const std::vector<DocumentsChainMetadataItemPg>& chain_metadata,
    const std::string& action_type) const {

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
        if (action_type == "signed and approved") {
            notification_text = fmt::format("Документ '{}' был подписан и утвержден пользователем {}.", doc_name, employee_name);
        } else if (action_type == "approved") {
            notification_text = fmt::format("Документ '{}' был утвержден пользователем {}.", doc_name, employee_name);
        } else if (action_type == "rejected") {
            notification_text = fmt::format("Документ '{}' был отклонен пользователем {}.", doc_name, employee_name);
        }

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

void AppendDocumentsChainUpdate(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsChainUpdateHandler>();
}

}  // namespace views::v1::documents::chain::update