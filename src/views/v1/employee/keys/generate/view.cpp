#define V1_EMPLOYEE_KEYS_GENERATE

#include "view.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cctype>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "definitions/all.hpp"

using json = nlohmann::json;

namespace views::v1::employee::keys::generate {

namespace {

// Функция для генерации RSA-2048 ключевой пары
struct KeyPair {
  std::string private_key;
  std::string public_key;
  std::string public_key_hash;
};

KeyPair GenerateRSAKeyPair(std::string_view signature_password) {
  KeyPair result;

  // Генерируем RSA ключ
  EVP_PKEY* pkey = EVP_PKEY_new();
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);

  if (EVP_PKEY_keygen_init(ctx) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    throw std::runtime_error("Failed to initialize key generation");
  }

  if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    throw std::runtime_error("Failed to set RSA key size");
  }

  if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    throw std::runtime_error("Failed to generate RSA key");
  }

  EVP_PKEY_CTX_free(ctx);

  // Экспортируем приватный ключ в PEM с шифрованием signature_password
  BIO* private_bio = BIO_new(BIO_s_mem());
  const unsigned char* kstr = reinterpret_cast<const unsigned char*>(signature_password.data());
  int klen = static_cast<int>(signature_password.size());
  if (PEM_write_bio_PrivateKey(private_bio, pkey, EVP_aes_256_cbc(), kstr, klen,
                                nullptr, nullptr) != 1) {
    BIO_free(private_bio);
    EVP_PKEY_free(pkey);
    throw std::runtime_error("Failed to write private key");
  }

  char* private_key_data = nullptr;
  long private_key_len = BIO_get_mem_data(private_bio, &private_key_data);
  result.private_key = std::string(private_key_data, private_key_len);
  BIO_free(private_bio);

  // Экспортируем публичный ключ в PEM
  BIO* public_bio = BIO_new(BIO_s_mem());
  if (PEM_write_bio_PUBKEY(public_bio, pkey) != 1) {
    BIO_free(public_bio);
    EVP_PKEY_free(pkey);
    throw std::runtime_error("Failed to write public key");
  }

  char* public_key_data = nullptr;
  long public_key_len = BIO_get_mem_data(public_bio, &public_key_data);
  result.public_key = std::string(public_key_data, public_key_len);
  BIO_free(public_bio);

  // Вычисляем SHA-256 хэш публичного ключа
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(result.public_key.c_str()),
         result.public_key.length(), hash);

  // Преобразуем в hex строку
  char hex_hash[SHA256_DIGEST_LENGTH * 2 + 1];
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    sprintf(hex_hash + (i * 2), "%02x", hash[i]);
  }
  hex_hash[SHA256_DIGEST_LENGTH * 2] = 0;
  result.public_key_hash = std::string(hex_hash);

  EVP_PKEY_free(pkey);

  return result;
}

class EmployeeKeysGenerateHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-keys-generate";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Employee keys generate handler schema
additionalProperties: false
properties: {}
)");
  }

  EmployeeKeysGenerateHandler(
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

    auto signature_password = request.GetArg("signature_password");
    if (signature_password.empty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"signature_password is required"}.ToJsonString();
    }
    if (signature_password.size() != 6 ||
        !std::all_of(signature_password.begin(), signature_password.end(),
                     [](unsigned char c) { return std::isdigit(c); })) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"signature_password must be exactly 6 digits"}.ToJsonString();
    }

    LOG_INFO() << "Generating NEP keys for user: " << user_id;

    // Проверяем, не существуют ли уже ключи
    auto existing_keys =
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "SELECT employee_id FROM working_day_" + company_id +
                ".employee_keys WHERE employee_id = $1",
            user_id);
    if (!existing_keys.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Keys already exist. Please delete old keys first."}.ToJsonString();
    }

    // Генерируем новую пару ключей (приватный ключ шифруется signature_password)
    KeyPair keys;
    try {
      keys = GenerateRSAKeyPair(signature_password);
    } catch (const std::exception& e) {
      LOG_ERROR() << "Failed to generate RSA keys: " << e.what();
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kInternalServerError);
      return ErrorMessage{"Failed to generate RSA keys"}.ToJsonString();
    }

    // Сохраняем ключи и пароль подписи в БД в одной транзакции
    try {
      auto trx = pg_cluster_->Begin(
          "employee_keys_generate",
          userver::storages::postgres::ClusterHostType::kMaster, {});

      trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".employee_keys "
              "(employee_id, private_key, public_key, public_key_hash) "
              "VALUES ($1, $2, $3, $4)",
          user_id, keys.private_key, keys.public_key, keys.public_key_hash);

      trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".employee_signature_passwords "
              "(employee_id, signature_password) "
              "VALUES ($1, $2)",
          user_id, signature_password);

      trx.Commit();
    } catch (const std::exception& e) {
      LOG_ERROR() << "Failed to save keys to database: " << e.what();
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kInternalServerError);
      return ErrorMessage{"Failed to save keys to database"}.ToJsonString();
    }

    // Формируем ответ
    EmployeeKeysGenerateResponse response;
    response.public_key = keys.public_key;
    response.public_key_hash = keys.public_key_hash;

    LOG_INFO() << "Successfully generated NEP keys for user: " << user_id;

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendEmployeeKeysGenerate(
    userver::components::ComponentList& component_list) {
  component_list.Append<EmployeeKeysGenerateHandler>();
}

}  // namespace views::v1::employee::keys::generate
