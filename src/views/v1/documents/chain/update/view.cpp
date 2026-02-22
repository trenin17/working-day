#define V1_DOCUMENTS_CHAIN_UPDATE

#include "view.hpp"

#include <cstring>
#include <optional>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

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

using json = nlohmann::json;

namespace views::v1::documents::chain::update {

struct MetadataContainer {
    std::vector<DocumentsChainMetadataItemPg> chain_metadata;
};

namespace {

static int SignaturePasswordCb(char* buf, int size, int /*rwflag*/, void* userdata) {
  const std::string* pass = static_cast<const std::string*>(userdata);
  int len = std::min(size - 1, static_cast<int>(pass->size()));
  if (len > 0) {
    std::memcpy(buf, pass->data(), len);
  }
  buf[len] = '\0';
  return len;
}

std::optional<std::string> DecryptPrivateKey(const std::string& encrypted_pem,
                                             const std::string& signature_password) {
  BIO* bio = BIO_new_mem_buf(encrypted_pem.data(), static_cast<int>(encrypted_pem.size()));
  if (!bio) return std::nullopt;
  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(
      bio, nullptr, &SignaturePasswordCb,
      const_cast<std::string*>(&signature_password));
  BIO_free(bio);
  if (!pkey) return std::nullopt;
  bio = BIO_new(BIO_s_mem());
  if (!bio) {
    EVP_PKEY_free(pkey);
    return std::nullopt;
  }
  if (PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
    BIO_free(bio);
    EVP_PKEY_free(pkey);
    return std::nullopt;
  }
  char* data = nullptr;
  long len = BIO_get_mem_data(bio, &data);
  std::string result(data, len);
  BIO_free(bio);
  EVP_PKEY_free(pkey);
  return result;
}

struct EmployeeKeysChain {
  std::string private_key;
  std::string public_key;
  std::string public_key_hash;
};

struct EmployeeInfoChain {
  std::string name;
  std::string surname;
  std::optional<std::string> patronymic;
};

class DocumentsChainUpdateHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-chain-update";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Chain update handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service for regular signature (document/sign)
    pyservice-nep-sign-url:
        type: string
        description: Url of python service for NEP signature (document/nep-sign)
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
        pyservice_url_(config["pyservice-url"].As<std::string>()),
        pyservice_nep_sign_url_(config["pyservice-nep-sign-url"].As<std::string>()) {}

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

    if (request_body.approval_status == 3) { // REJECT
        current_it->status = 2;
        SendNotifications(company_id, document_id, user_id, chain_metadata, "rejected");
    }
    else if (request_body.approval_status == 2 && current_it->requires_signature) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Document requires a signature"}.ToJsonString();

    } else if ((!request_body.approval_status || request_body.approval_status == 1) && !current_it->requires_signature) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Document doesn't require a signature"}.ToJsonString();
    }
    // approve a document that does not require a signature
    else if (!current_it->requires_signature && request_body.approval_status == 2) {
        current_it->status = 1;
        SendNotifications(company_id, document_id, user_id, chain_metadata, "approved");
    }
    // approve with KEP signature (already uploaded via upload_signature, front sends signature_id)
    else if (current_it->requires_signature == 1 && request_body.approval_status == 0) {
        if (!request_body.signature_id.has_value()) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return ErrorMessage{"signature_id is required for KEP signing"}.ToJsonString();
        }
        const std::string& signature_id = request_body.signature_id.value();
        auto sig_check = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT id FROM working_day_" + company_id +
                ".document_signatures "
                "WHERE id = $1 AND document_id = $2 AND employee_id = $3 AND signature_type = $4",
            signature_id, document_id, user_id, "kep");
        if (sig_check.IsEmpty()) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return ErrorMessage{"KEP signature not found or invalid. Upload signature via /v1/documents/upload_signature first."}.ToJsonString();
        }

        result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "UPDATE working_day_" + company_id + ".employee_document "
            "SET signed = true "
            "WHERE employee_id = $1 AND document_id = $2",
            user_id, document_id);

        current_it->status = 1;
        SendNotifications(company_id, document_id, user_id, chain_metadata, "signed and approved");
    }
    // approve with unqualified signature (NEP) — call document/nep-sign, same logic as nep_sign view
    else if (current_it->requires_signature == 2 && request_body.approval_status == 1) {
        if (!request_body.signature_password.has_value() || request_body.signature_password->empty()) {
          request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
          return ErrorMessage{"signature_password is required for NEP signing"}.ToJsonString();
        }
        const std::string& signature_password = request_body.signature_password.value();

        auto keys_result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT private_key, public_key, public_key_hash FROM working_day_" + company_id +
                ".employee_keys WHERE employee_id = $1",
            user_id);
        if (keys_result.IsEmpty()) {
          request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
          return ErrorMessage{"Keys not found. Please generate keys first."}.ToJsonString();
        }
        auto keys = keys_result.AsSingleRow<EmployeeKeysChain>(userver::storages::postgres::kRowTag);

        auto stored_password_result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT signature_password FROM working_day_" + company_id +
                ".employee_signature_passwords WHERE employee_id = $1",
            user_id);
        if (stored_password_result.IsEmpty()) {
          request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
          return ErrorMessage{"Signature password not found. Please generate keys first."}.ToJsonString();
        }
        if (stored_password_result.AsSingleRow<std::string>() != signature_password) {
          request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
          return ErrorMessage{"Wrong signature_password"}.ToJsonString();
        }

        auto employee_info_result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT name, surname, patronymic FROM working_day_" + company_id +
                ".employees WHERE id = $1",
            user_id);
        auto employee_info = employee_info_result.AsSingleRow<EmployeeInfoChain>(userver::storages::postgres::kRowTag);
        std::string employee_full_name = employee_info.surname + " " + employee_info.name;
        if (employee_info.patronymic.has_value()) {
          employee_full_name += " " + employee_info.patronymic.value();
        }

        auto decrypted_private_key = DecryptPrivateKey(keys.private_key, signature_password);
        if (!decrypted_private_key.has_value()) {
          request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
          return ErrorMessage{"Wrong signature_password"}.ToJsonString();
        }

        PyserviceNepSignRequest pyservice_request;
        pyservice_request.document_id = document_id;
        pyservice_request.employee_id = user_id;
        pyservice_request.employee_name = employee_full_name;
        pyservice_request.private_key = decrypted_private_key.value();
        pyservice_request.public_key = keys.public_key;

        auto resp = http_client_.CreateRequest()
                        .post(pyservice_nep_sign_url_)
                        .data(pyservice_request.ToJsonString())
                        .retry(2)
                        .timeout(std::chrono::milliseconds{10000})
                        .perform();
        resp->raise_for_status();

        auto response_json = json::parse(resp->body());
        std::string signature_path = response_json["signature_path"];
        std::string timestamp = response_json["timestamp"];
        std::string public_key_hash = response_json["public_key_hash"];

        auto signature_id = userver::utils::generators::GenerateUuid();
        json metadata;
        metadata["timestamp"] = timestamp;
        metadata["user_name"] = employee_full_name;
        metadata["user_id"] = user_id;

        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO working_day_" + company_id + ".document_signatures "
            "(id, document_id, employee_id, signature_path, signature_metadata, public_key_hash, signature_type) "
            "VALUES ($1, $2, $3, $4, $5::jsonb, $6, $7)",
            signature_id, document_id, user_id, signature_path,
            metadata.dump(), public_key_hash, "nep");

        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "UPDATE working_day_" + company_id + ".employee_document "
            "SET signed = true, updated_ts = NOW() "
            "WHERE employee_id = $1 AND document_id = $2",
            user_id, document_id);

        current_it->status = 1;
        SendNotifications(company_id, document_id, user_id, chain_metadata, "signed and approved");
    } else {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Bad request."}.ToJsonString();
    }

    size_t element_index = current_it - chain_metadata.begin();
    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id + ".documents "
        "SET chain_metadata_new[" + std::to_string(element_index + 1) + "] = $2 "
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
        "SELECT chain_metadata_new "
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
  std::string pyservice_nep_sign_url_;

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