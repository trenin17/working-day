#define V1_DOCUMENTS_NEP_SIGN

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/http/url.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "definitions/all.hpp"

using json = nlohmann::json;

namespace views::v1::documents::nep_sign {

namespace {

struct EmployeeKeys {
  std::string private_key;
  std::string public_key;
  std::string public_key_hash;
};

struct EmployeeInfo {
  std::string name;
  std::string surname;
  std::optional<std::string> patronymic;
};

class DocumentsNepSignHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-nep-sign";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Documents NEP sign handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

  DocumentsNepSignHandler(
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

    auto document_id = request.GetArg("document_id");
    LOG_INFO() << "Signing document with NEP: " << document_id
               << " by user: " << user_id;

    DocumentsNepSignRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    // Проверяем существование документа
    auto document_check =
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT id FROM working_day_" + company_id +
                ".documents "
                "WHERE id = $1",
            document_id);

    if (document_check.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Document not found"}.ToJsonString();
    }

    // Получаем ключи сотрудника
    auto keys_result =
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT private_key, public_key, public_key_hash "
            "FROM working_day_" +
                company_id +
                ".employee_keys "
                "WHERE employee_id = $1",
            user_id);

    if (keys_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Keys not found. Please generate keys first."}.ToJsonString();
    }

    auto keys =
        keys_result.AsSingleRow<EmployeeKeys>(userver::storages::postgres::kRowTag);

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

    // Отправляем запрос в Python сервис для подписания
    PyserviceNepSignRequest pyservice_request;
    pyservice_request.document_id = document_id;
    pyservice_request.employee_id = user_id;
    pyservice_request.employee_name = employee_full_name;
    pyservice_request.private_key = keys.private_key;
    pyservice_request.public_key = keys.public_key;
    pyservice_request.reason = request_body.reason;
    pyservice_request.location = request_body.location;

    auto resp = http_client_.CreateRequest()
                    .post("http://localhost:3000/document/nep-sign")
                    .data(pyservice_request.ToJsonString())
                    .retry(2)
                    .timeout(std::chrono::milliseconds{10000})
                    .perform();

    resp->raise_for_status();

    // Парсим ответ от Python сервиса
    auto response_json = json::parse(resp->body());

    std::string signature_path = response_json["signature_path"];
    std::string timestamp = response_json["timestamp"];
    std::string public_key_hash = response_json["public_key_hash"];

    // Генерируем ID для подписи
    auto signature_id = userver::utils::generators::GenerateUuid();

    // Сохраняем метаданные подписи в базу данных
    json metadata;
    metadata["timestamp"] = timestamp;
    metadata["user_name"] = employee_full_name;
    metadata["user_id"] = user_id;
    if (request_body.reason.has_value()) {
      metadata["reason"] = request_body.reason.value();
    }
    if (request_body.location.has_value()) {
      metadata["location"] = request_body.location.value();
    }

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".document_signatures "
            "(id, document_id, employee_id, signature_path, "
            "signature_metadata, public_key_hash) "
            "VALUES ($1, $2, $3, $4, $5::jsonb, $6)",
        signature_id, document_id, user_id, signature_path,
        metadata.dump(), public_key_hash);

    // Обновляем статус подписания в employee_document
    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id +
            ".employee_document "
            "SET signed = true, updated_ts = NOW() "
            "WHERE employee_id = $1 AND document_id = $2",
        user_id, document_id);

    // Формируем ответ
    DocumentsNepSignResponse result;
    result.signature_id = signature_id;
    result.signature_path = signature_path;
    result.timestamp = timestamp;
    result.public_key_hash = public_key_hash;

    LOG_INFO() << "Successfully signed document: " << document_id
               << " with signature: " << signature_id;

    return result.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsNepSign(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsNepSignHandler>();
}

}  // namespace views::v1::documents::nep_sign
