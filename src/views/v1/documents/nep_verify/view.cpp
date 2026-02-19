#define V1_DOCUMENTS_NEP_VERIFY

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "definitions/all.hpp"

using json = nlohmann::json;

namespace views::v1::documents::nep_verify {

namespace {

struct SignatureInfo {
  std::string id;
  std::string document_id;
  std::string employee_id;
  std::string signature_path;
  std::string signature_metadata;
  std::string public_key_hash;
  std::string signature_type;
};

struct EmployeeKeys {
  std::string public_key;
};

class DocumentsNepVerifyHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-nep-verify";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Documents NEP verify handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

  DocumentsNepVerifyHandler(
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

    auto user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto signature_id = request.GetArg("signature_id");
    LOG_INFO() << "Verifying NEP signature: " << signature_id
               << " by user: " << user_id;

    // Получаем информацию о подписи
    auto signature_result =
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT id, document_id, employee_id, signature_path, "
            "signature_metadata::text, public_key_hash, signature_type "
            "FROM working_day_" +
                company_id +
                ".document_signatures "
                "WHERE id = $1",
            signature_id);

    if (signature_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Signature not found"}.ToJsonString();
    }

    auto signature =
        signature_result.AsSingleRow<SignatureInfo>(
            userver::storages::postgres::kRowTag);

    if (signature.signature_type != "nep") {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Signature type is not NEP"}.ToJsonString();
    }

    // Получаем публичный ключ сотрудника, который подписал документ
    auto keys_result =
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT public_key "
            "FROM working_day_" +
                company_id +
                ".employee_keys "
                "WHERE employee_id = $1",
            signature.employee_id);

    if (keys_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Public key not found. Cannot verify signature."}.ToJsonString();
    }

    auto keys =
        keys_result.AsSingleRow<EmployeeKeys>(
            userver::storages::postgres::kRowTag);

    // Отправляем запрос в Python сервис для проверки подписи
    PyserviceNepVerifyRequest pyservice_request;
    pyservice_request.document_id = signature.document_id;
    pyservice_request.signature_path = signature.signature_path;
    pyservice_request.public_key = keys.public_key;

    auto resp = http_client_.CreateRequest()
                    .post(pyservice_url_)
                    .data(pyservice_request.ToJsonString())
                    .retry(2)
                    .timeout(std::chrono::milliseconds{10000})
                    .perform();

    resp->raise_for_status();

    // Парсим ответ от Python сервиса
    auto response_json = json::parse(resp->body());

    bool valid = response_json["valid"];
    bool integrity_ok = response_json["integrity_ok"];
    bool signature_ok = response_json["signature_ok"];
    std::string message = response_json["message"];

    // Парсим метаданные подписи
    auto metadata_json = json::parse(signature.signature_metadata);
    std::string signer_name = metadata_json.value("user_name", "");
    std::string signature_timestamp = metadata_json.value("timestamp", "");

    // Формируем ответ
    DocumentsNepVerifyResponse result;
    result.valid = valid;
    result.integrity_ok = integrity_ok;
    result.signature_ok = signature_ok;
    result.message = message;
    result.verified_at =
        userver::utils::datetime::Timestring(std::chrono::system_clock::now(),
                                             "UTC", "%Y-%m-%dT%H:%M:%S");
    result.signer_name = signer_name;
    result.signature_timestamp = signature_timestamp;

    LOG_INFO() << "Verification result for signature " << signature_id
               << ": valid=" << valid;

    return result.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsNepVerify(
    userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsNepVerifyHandler>();
}

}  // namespace views::v1::documents::nep_verify
