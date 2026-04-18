#define V1_DOCUMENTS_SIGN

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include <nlohmann/json.hpp>

#include <definitions/all.hpp>

namespace views::v1::documents::sign {

namespace {

class DocumentInfo {
 public:
  std::string id, name, type;
  int sign_required;
  std::optional<std::string> description;
  std::optional<std::string> parent_id;
};

class DocumentsCreateStampForNepHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName =
      "handler-v1-documents-create-stamp-for-nep";

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Create visual NEP stamp on PDF (python service)
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

  DocumentsCreateStampForNepHandler(
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
        "SELECT id, name, type, sign_required, description, parent_id "
        "FROM working_day_" +
            company_id +
            ".documents "
            "WHERE id = $1",
        document_id);
    if (result.IsEmpty()) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Document not found"}.ToJsonString();
    }
    auto document_info =
        result.AsSingleRow<DocumentInfo>(userver::storages::postgres::kRowTag);

    if (document_info.sign_required == 0) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Document doesn't require sign"}.ToJsonString();
    }

    const bool has_parent = document_info.parent_id.has_value() &&
                            !document_info.parent_id->empty();

    result = pg_cluster_->Execute(
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
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"No NEP signatures for this document"}
          .ToJsonString();
    }

    // Не используем JsonCompatible::ToJsonString для signers: у JsonCompatible
    // копирующий/перемещающий конструкторы сбрасывают регистрацию полей, из‑за чего
    // vector<PyserviceNepStampSigner> после push_back сериализовался как [null].
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
               << " pyservice_url=" << pyservice_url_
               << " signers_count=" << signers_json.size()
               << " signed_file_key=" << signed_file_key;
    LOG_INFO() << "create-stamp-for-nep pyservice JSON body: " << py_json;

    auto response = http_client_.CreateRequest()
                        .post(pyservice_url_)
                        .headers(
                            {{userver::http::headers::kContentType,
                              userver::http::content_type::kApplicationJson
                                  .ToString()}})
                        .data(std::move(py_json))
                        .retry(2)
                        .timeout(std::chrono::milliseconds{5000})
                        .perform();
    response->raise_for_status();

    // Предыдущая визуальная копия (ребёнок с parent_id = корень). У корня parent_id = id,
    // иначе DELETE WHERE parent_id = $1 снёс бы и сам корень.
    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "DELETE FROM working_day_" + company_id +
                             ".documents WHERE parent_id = $1 AND id <> $1",
                         document_id);

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
        signed_file_key, document_info.name, document_info.type,
        document_info.sign_required, document_info.description, document_id);

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".employee_document (employee_id, document_id, signed) "
            "VALUES ($1, $2, $3)",
        user_id, signed_file_key, true);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsSign(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsCreateStampForNepHandler>();
}

}  // namespace views::v1::documents::sign
