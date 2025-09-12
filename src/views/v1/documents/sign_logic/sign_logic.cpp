#define V1_DOCUMENTS_SIGN

#include "sign_logic.hpp"

#include <userver/clients/http/client.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include <string>

#include "definitions/all.hpp"

namespace views::v1::documents::sign::logic {

class DocumentInfo {
 public:
  std::string id, name, type;
  bool sign_required;
  std::optional<std::string> description;
};

std::string SignDocument(const DocumentSignParams& params) {
    LOG_INFO() << "SignDocument: " << params.document_id << ", [] = " << params.company_id;
    auto result = params.pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, name, type, sign_required, description "
        "FROM working_day_" + params.company_id +
        ".documents "
        "WHERE id = $1",
        params.document_id);
    auto document_info =
        result.AsSingleRow<DocumentInfo>(userver::storages::postgres::kRowTag);

    if (!document_info.sign_required) {
        throw std::runtime_error("Document doesn't require sign");
    }

    result = params.pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, name, surname, patronymic, photo_link, subcompany "
        "FROM working_day_" + params.company_id +
        ".employees "
        "WHERE id = $1",
        params.user_id);
    auto employee_info = result.AsSingleRow<ListEmployeeWithSubcompany>(
        userver::storages::postgres::kRowTag);

    PyserviceDocumentSignRequest py_request;
    py_request.employee_id = params.user_id;
    py_request.employee_name = employee_info.name;
    py_request.employee_surname = employee_info.surname;
    py_request.employee_patronymic = employee_info.patronymic;
    py_request.subcompany = employee_info.subcompany;
    py_request.file_key = params.document_id;
    py_request.signed_file_key =
        userver::utils::generators::GenerateUuid() + ".pdf";

    auto response = params.http_client.CreateRequest()
                        .post(params.pyservice_url)
                        .data(py_request.ToJsonString())
                        .retry(2)
                        .timeout(std::chrono::milliseconds{5000})
                        .perform();
    response->raise_for_status();

    params.pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "DELETE FROM working_day_" + params.company_id +
            ".employee_document "
            "WHERE employee_id = $1 AND document_id = $2",
        params.user_id, params.document_id);

    params.pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + params.company_id +
            ".documents(id, name, type, "
            "sign_required, description, parent_id) "
            "VALUES($1, $2, $3, $4, $5, $6)",
        py_request.signed_file_key, document_info.name, document_info.type,
        true, document_info.description, params.document_id);

    params.pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + params.company_id +
            ".employee_document (employee_id, document_id, signed) "
            "VALUES ($1, $2, $3)",
        params.user_id, py_request.signed_file_key, true);

    return py_request.signed_file_key;
}

} // namespace views::v1::documents::sign::logic