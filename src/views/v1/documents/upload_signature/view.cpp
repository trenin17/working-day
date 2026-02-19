#define V1_DOCUMENTS_UPLOAD_SIGNATURE

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
#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::documents::upload_signature {

namespace {

struct EmployeeInfo {
    std::string name;
    std::string surname;
    std::optional<std::string> patronymic;
};

class DocumentsUploadSignatureHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-upload-signature";

  DocumentsUploadSignatureHandler(
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

    DocumentsUploadSignatureRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    // TODO: if NEP - check + other logic
    if (request_body.signature_type == "nep") {
        request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Document signature type is not supported"}.ToJsonString();
    } else if (request_body.signature_type == "kep") {
        auto perm_result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT permission_value "
            "FROM working_day_" +
                company_id +
                ".employee_permissions "
            "WHERE employee_id = $1 AND permission_type = 'can_upload_kep_signature'",
            user_id);
        if (perm_result.IsEmpty() || perm_result.AsSingleRow<int>() == 0) {
            request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kForbidden);
            return ErrorMessage{"Insufficient rights"}.ToJsonString();
        }
    }

    auto document_check = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id FROM working_day_" + company_id +
            ".documents "
            "WHERE id = $1",
        request_body.document_id);

    if (document_check.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Document not found"}.ToJsonString();
    }

    // Получаем информацию о сотруднике
    auto employee_info =
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT name, surname, patronymic "
            "FROM working_day_" +
                company_id +
                ".employees "
                "WHERE id = $1",
            user_id)
            .AsSingleRow<EmployeeInfo>(userver::storages::postgres::kRowTag);


    // Формируем полное имя сотрудника
    std::string employee_full_name = employee_info.surname + " " + employee_info.name;
    if (employee_info.patronymic.has_value()) {
      employee_full_name += " " + employee_info.patronymic.value();
    }

    json metadata;
    // TODO: metadata["timestamp"] = request_body.timestamp;
    metadata["user_name"] = employee_full_name;
    metadata["user_id"] = user_id;
    // TODO: metadata["reason"] = request_body.reason;
    // TODO: metadata["location"] = request_body.location;
    std::string signature_path = request_body.document_id + "_" + user_id + request_body.extension;
    metadata["signature_path"] = signature_path;
    metadata["detached"] = true;

    // Генерируем ID для подписи
    auto signature_id = userver::utils::generators::GenerateUuid();



    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".document_signatures "
            "(id, document_id, employee_id, signature_path, "
            "signature_metadata, signature_type) "
            "VALUES ($1, $2, $3, $4, $5::jsonb, $6)",
        signature_id, request_body.document_id, user_id, signature_path,
        metadata.dump(), request_body.signature_type);

    auto document_id =
        userver::utils::generators::GenerateUuid() + request_body.extension;
    auto upload_link = utils::s3_presigned_links::GenerateDocumentPresignedLink(
        document_id, utils::s3_presigned_links::Upload);

    DocumentsUploadSignatureResponse response;
    response.signature_id = signature_id;
    response.url = upload_link;
    return response.ToJsonString();
  }
 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;

};

}  // namespace

void AppendDocumentsUploadSignatureView(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsUploadSignatureHandler>();
}

}  // namespace views::v1::documents::upload_signature