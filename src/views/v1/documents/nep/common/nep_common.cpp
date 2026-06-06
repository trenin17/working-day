#include "views/v1/documents/nep/common/nep_common.hpp"

#include <cstring>
#include <optional>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <userver/http/common_headers.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/uuid4.hpp>

#include <nlohmann/json.hpp>

#define V1_DOCUMENTS_NEP_SIGN
#define V1_DOCUMENTS_CREATE_STAMP_FOR_NEP
#include "definitions/all.hpp"

namespace views::v1::documents::nep::common {

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

class DocumentInfo {
 public:
  std::string id, name, type;
  int sign_required;
  std::optional<std::string> description;
  std::optional<std::string> parent_id;
};

}  // namespace

std::optional<JsonErrorResponse> RunNepSign(
    userver::storages::postgres::ClusterPtr pg_cluster,
    userver::clients::http::Client& http_client,
    const std::string& pyservice_nep_sign_url, const std::string& company_id,
    const std::string& user_id, const std::string& document_id,
    const std::string& signature_password,
    const std::optional<std::string>& reason,
    const std::optional<std::string>& location, NepSignResult& out) {
  using userver::server::http::HttpStatus;

  auto document_check =
      pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                          "SELECT id FROM working_day_" + company_id +
                              ".documents "
                              "WHERE id = $1",
                          document_id);

  if (document_check.IsEmpty()) {
    return JsonErrorResponse{HttpStatus::kNotFound,
                             ErrorMessage{"Document not found"}.ToJsonString()};
  }

  auto keys_result =
      pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                          "SELECT private_key, public_key, public_key_hash "
                          "FROM working_day_" +
                              company_id +
                              ".employee_keys "
                              "WHERE employee_id = $1",
                          user_id);

  if (keys_result.IsEmpty()) {
    return JsonErrorResponse{
        HttpStatus::kBadRequest,
        ErrorMessage{"Keys not found. Please generate keys first."}.ToJsonString()};
  }

  auto keys = keys_result.AsSingleRow<EmployeeKeys>(userver::storages::postgres::kRowTag);

  auto stored_password_result =
      pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                          "SELECT signature_password FROM working_day_" + company_id +
                              ".employee_signature_passwords WHERE employee_id = $1",
                          user_id);
  if (stored_password_result.IsEmpty()) {
    return JsonErrorResponse{
        HttpStatus::kBadRequest,
        ErrorMessage{"Signature password not found. Please generate keys first."}
            .ToJsonString()};
  }
  std::string stored_signature_password =
      stored_password_result.AsSingleRow<std::string>();
  if (stored_signature_password != signature_password) {
    return JsonErrorResponse{HttpStatus::kUnauthorized,
                             ErrorMessage{"Wrong signature_password"}.ToJsonString()};
  }

  auto employee_info =
      pg_cluster
          ->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                    "SELECT name, surname, patronymic "
                    "FROM working_day_" +
                        company_id +
                        ".employees "
                        "WHERE id = $1",
                    user_id)
          .AsSingleRow<EmployeeInfo>(userver::storages::postgres::kRowTag);

  std::string employee_full_name = employee_info.surname + " " + employee_info.name;
  if (employee_info.patronymic.has_value()) {
    employee_full_name += " " + employee_info.patronymic.value();
  }

  auto decrypted_private_key =
      DecryptPrivateKey(keys.private_key, signature_password);
  if (!decrypted_private_key.has_value()) {
    return JsonErrorResponse{HttpStatus::kUnauthorized,
                             ErrorMessage{"Wrong signature_password"}.ToJsonString()};
  }

  PyserviceNepSignRequest pyservice_request;
  pyservice_request.document_id = document_id;
  pyservice_request.employee_id = user_id;
  pyservice_request.employee_name = employee_full_name;
  pyservice_request.private_key = decrypted_private_key.value();
  pyservice_request.public_key = keys.public_key;
  pyservice_request.reason = reason;
  pyservice_request.location = location;

  auto resp = http_client.CreateRequest()
                  .post(pyservice_nep_sign_url)
                  .data(pyservice_request.ToJsonString())
                  .retry(2)
                  .timeout(std::chrono::milliseconds{10000})
                  .perform();

  resp->raise_for_status();

  auto response_json = nlohmann::json::parse(resp->body());

  std::string signature_path = response_json["signature_path"];
  std::string timestamp = response_json["timestamp"];
  std::string public_key_hash = response_json["public_key_hash"];

  auto signature_id = userver::utils::generators::GenerateUuid();

  nlohmann::json metadata;
  metadata["timestamp"] = timestamp;
  metadata["user_name"] = employee_full_name;
  metadata["user_id"] = user_id;
  if (reason.has_value()) {
    metadata["reason"] = reason.value();
  }
  if (location.has_value()) {
    metadata["location"] = location.value();
  }

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "INSERT INTO working_day_" + company_id +
                          ".document_signatures "
                          "(id, document_id, employee_id, signature_path, "
                          "signature_metadata, public_key_hash, signature_type) "
                          "VALUES ($1, $2, $3, $4, $5::jsonb, $6, $7)",
                      signature_id, document_id, user_id, signature_path,
                      metadata.dump(), public_key_hash, "nep");

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "UPDATE working_day_" + company_id +
                          ".employee_document "
                          "SET signed = true, updated_ts = NOW() "
                          "WHERE employee_id = $1 AND document_id = $2",
                      user_id, document_id);

  out.signature_id = std::move(signature_id);
  out.signature_path = std::move(signature_path);
  out.timestamp = std::move(timestamp);
  out.public_key_hash = std::move(public_key_hash);
  return std::nullopt;
}

std::optional<JsonErrorResponse> RunCreateStampForNep(
    userver::storages::postgres::ClusterPtr pg_cluster,
    userver::clients::http::Client& http_client,
    const std::string& pyservice_create_stamp_url, const std::string& company_id,
    const std::string& user_id, const std::string& document_id) {
  using userver::server::http::HttpStatus;

  auto result = pg_cluster->Execute(
      userver::storages::postgres::ClusterHostType::kSlave,
      "SELECT id, name, type, sign_required, description, parent_id "
      "FROM working_day_" +
          company_id +
          ".documents "
          "WHERE id = $1",
      document_id);
  if (result.IsEmpty()) {
    return JsonErrorResponse{HttpStatus::kNotFound,
                             ErrorMessage{"Document not found"}.ToJsonString()};
  }
  auto document_info =
      result.AsSingleRow<DocumentInfo>(userver::storages::postgres::kRowTag);

  if (document_info.sign_required == 0) {
    return JsonErrorResponse{
        HttpStatus::kBadRequest,
        ErrorMessage{"Document doesn't require sign"}.ToJsonString()};
  }

  const bool has_parent = document_info.parent_id.has_value() &&
                          !document_info.parent_id->empty();

  result = pg_cluster->Execute(
      userver::storages::postgres::ClusterHostType::kSlave,
      "SELECT e.id, e.name, e.surname, e.patronymic, e.subcompany "
      "FROM ( "
      "  SELECT employee_id, MIN(created_ts) AS first_ts "
      "  FROM working_day_" +
          company_id +
          ".document_signatures "
      "  WHERE document_id = $1 AND signature_type = 'nep' "
      "  GROUP BY employee_id "
      ") AS ds "
      "JOIN working_day_" +
          company_id +
          ".employees e ON e.id = ds.employee_id "
      "ORDER BY ds.first_ts ASC",
      document_id);

  if (result.IsEmpty()) {
    return JsonErrorResponse{
        HttpStatus::kBadRequest,
        ErrorMessage{"No NEP signatures for this document"}.ToJsonString()};
  }

  const std::string signed_file_key =
      userver::utils::generators::GenerateUuid() + ".pdf";
  const bool is_first_stamp = !has_parent;
  std::string organization;

  nlohmann::json signers_json = nlohmann::json::array();
  for (const auto& row : result) {
    auto [eid, name, surname, patronymic, subcompany] =
        row.As<std::string, std::string, std::string,
               std::optional<std::string>, std::string>();
    nlohmann::json sj;
    sj["employee_id"] = eid;
    sj["name"] = name;
    sj["surname"] = surname;
    if (patronymic.has_value() && !patronymic->empty()) {
      sj["patronymic"] = *patronymic;
    }
    signers_json.push_back(std::move(sj));
    if (organization.empty()) {
      organization = subcompany;
    }
    LOG_INFO() << "create-stamp-for-nep signer: employee_id=" << eid
               << " surname=" << surname << " name=" << name;
  }

  nlohmann::json body;
  body["file_key"] = document_id;
  body["signed_file_key"] = signed_file_key;
  body["is_first_signature"] = is_first_stamp;
  body["organization"] = organization;
  body["signers"] = signers_json;

  std::string py_json = body.dump();
  LOG_INFO() << "create-stamp-for-nep: document_id=" << document_id
             << " signers_count=" << signers_json.size()
             << " signed_file_key=" << signed_file_key;
  LOG_INFO() << "create-stamp-for-nep pyservice JSON body: " << py_json;

  auto response = http_client.CreateRequest()
                      .post(pyservice_create_stamp_url)
                      .headers(
                          {{userver::http::headers::kContentType,
                            userver::http::content_type::kApplicationJson
                                .ToString()}})
                      .data(std::move(py_json))
                      .retry(2)
                      .timeout(std::chrono::milliseconds{5000})
                      .perform();
  response->raise_for_status();

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "DELETE FROM working_day_" + company_id +
                          ".documents WHERE parent_id = $1 AND id <> $1",
                      document_id);

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "DELETE FROM working_day_" + company_id +
                          ".employee_document "
                          "WHERE employee_id = $1 AND document_id = $2",
                      user_id, document_id);

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "INSERT INTO working_day_" + company_id +
                          ".documents(id, name, type, "
                          "sign_required, description, parent_id) "
                          "VALUES($1, $2, $3, $4, $5, $6)",
                      signed_file_key, document_info.name, document_info.type,
                      document_info.sign_required, document_info.description,
                      document_id);

  pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                      "INSERT INTO working_day_" + company_id +
                          ".employee_document (employee_id, document_id, signed) "
                          "VALUES ($1, $2, $3)",
                      user_id, signed_file_key, true);

  return std::nullopt;
}

std::optional<JsonErrorResponse> TryCreateStampAfterNepSign(
    userver::storages::postgres::ClusterPtr pg_cluster,
    userver::clients::http::Client& http_client,
    const std::string& pyservice_create_stamp_url, const std::string& company_id,
    const std::string& user_id, const std::string& document_id) {
  using userver::server::http::HttpStatus;

  auto res = pg_cluster->Execute(
      userver::storages::postgres::ClusterHostType::kSlave,
      "SELECT sign_required FROM working_day_" + company_id +
          ".documents WHERE id = $1",
      document_id);
  if (res.IsEmpty()) {
    return JsonErrorResponse{HttpStatus::kNotFound,
                             ErrorMessage{"Document not found"}.ToJsonString()};
  }
  const int sign_required = res.AsSingleRow<int>();
  if (sign_required == 0) {
    return std::nullopt;
  }
  return RunCreateStampForNep(pg_cluster, http_client, pyservice_create_stamp_url,
                              company_id, user_id, document_id);
}

}  // namespace views::v1::documents::nep::common
