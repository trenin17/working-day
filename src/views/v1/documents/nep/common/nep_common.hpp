#pragma once

#include <optional>
#include <string>

#include <userver/clients/http/client.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace views::v1::documents::nep::common {

/// Ответ об ошибке с телом JSON (как ErrorMessage::ToJsonString()).
struct JsonErrorResponse {
  userver::server::http::HttpStatus status;
  std::string body;
};

struct NepSignResult {
  std::string signature_id;
  std::string signature_path;
  std::string timestamp;
  std::string public_key_hash;
};

/// Полный цикл НЭП: Python nep-sign + INSERT в document_signatures + UPDATE employee_document.
/// При успехе заполняет `out`; при ошибке — возвращает ответ для HTTP.
[[nodiscard]] std::optional<JsonErrorResponse> RunNepSign(
    userver::storages::postgres::ClusterPtr pg_cluster,
    userver::clients::http::Client& http_client,
    const std::string& pyservice_nep_sign_url, const std::string& company_id,
    const std::string& user_id, const std::string& document_id,
    const std::string& signature_password,
    const std::optional<std::string>& reason,
    const std::optional<std::string>& location, NepSignResult& out);

/// Визуальный штамп НЭП (Python create-stamp-for-nep) + дочерний документ в БД.
/// Как ручка create-stamp-for-nep: требует sign_required != 0 и хотя бы одну NEP-подпись.
[[nodiscard]] std::optional<JsonErrorResponse> RunCreateStampForNep(
    userver::storages::postgres::ClusterPtr pg_cluster,
    userver::clients::http::Client& http_client,
    const std::string& pyservice_create_stamp_url, const std::string& company_id,
    const std::string& user_id, const std::string& document_id);

/// Если у документа sign_required != 0 — вызывает RunCreateStampForNep (после успешной НЭП).
[[nodiscard]] std::optional<JsonErrorResponse> TryCreateStampAfterNepSign(
    userver::storages::postgres::ClusterPtr pg_cluster,
    userver::clients::http::Client& http_client,
    const std::string& pyservice_create_stamp_url, const std::string& company_id,
    const std::string& user_id, const std::string& document_id);

}  // namespace views::v1::documents::nep::common
