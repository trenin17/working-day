#define V1_DOCUMENTS_SIGN

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

namespace views::v1::documents::sign {

namespace {

class DocumentInfo {
 public:
  std::string id, name, type;
  bool sign_required;
  std::optional<std::string> description;
};

class DocumentsSignHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-sign";

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

  DocumentsSignHandler(
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

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, name, type, sign_required, description "
        "FROM working_day_" +
            company_id +
            ".documents "
            "WHERE id = $1",
        document_id);
    auto document_info =
        result.AsSingleRow<DocumentInfo>(userver::storages::postgres::kRowTag);

    if (!document_info.sign_required) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Document doesn't require sign"}.ToJsonString();
    }

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
    py_request.signed_file_key =
        userver::utils::generators::GenerateUuid() + ".pdf";

    auto response = http_client_.CreateRequest()
                        .post(pyservice_url_)
                        .data(py_request.ToJsonString())
                        .retry(2)  // retry once in case of error
                        .timeout(std::chrono::milliseconds{5000})
                        .perform();  // start performing the request
    response->raise_for_status();

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "DELETE FROM working_day_" + company_id +
            ".employee_document "
            "WHERE employee_id = $1 AND document_id = $2",
        user_id, document_id);

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".documents(id, name, type, "
            "sign_required, description, parent_id) "
            "VALUES($1, $2, $3, $4, $5, $6)",
        py_request.signed_file_key, document_info.name, document_info.type,
        true, document_info.description, document_id);

    LOG_INFO() << "NEW ID " << py_request.signed_file_key;

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT id, name, type, sign_required, description "
        "FROM working_day_" +
            company_id +
            ".documents "
            "WHERE id = $1",
        py_request.signed_file_key);

    auto inserted_docs =
        result.AsSingleRow<DocumentInfo>(userver::storages::postgres::kRowTag);
    LOG_INFO() << "ROWS AFFECTED " << inserted_docs.id << " "
               << inserted_docs.name << " " << inserted_docs.type << " "
               << inserted_docs.sign_required << " "
               << inserted_docs.description.value_or("") << " " << document_id;

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".employee_document (employee_id, document_id, signed) "
            "VALUES ($1, $2, $3)",
        user_id, py_request.signed_file_key, true);

    LOG_INFO() << "&ROWS AFFECTED " << result.RowsAffected();

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsSign(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsSignHandler>();
}

}  // namespace views::v1::documents::sign