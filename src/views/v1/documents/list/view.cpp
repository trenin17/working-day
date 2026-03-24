#define V1_DOCUMENTS_LIST

#include "view.hpp"

#include <unordered_map>
#include <unordered_set>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>

#include "utils/s3_presigned_links.hpp"

namespace views::v1::documents::list {

namespace {

class DocumentsListHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-list";

  DocumentsListHandler(
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

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT d.id, d.name, "
            "d.type, d.sign_required, "
            "d.description, "
            "COALESCE(ed.signed, FALSE) AS signed, d.author_id, "
            "e.photo_link AS author_photo_url, "
            "NULL::TEXT as parent_id, d.created_ts, d.chain_metadata_new, d.visibility_status "
            "FROM working_day_" +
            company_id +
            ".documents d "
            "LEFT JOIN working_day_" +
            company_id + ".employee_document ed ON d.id = ed.document_id AND ed.employee_id = $1 "
            "LEFT JOIN working_day_" +
            company_id + ".employees e ON e.id = d.author_id "
            "WHERE ed.employee_id = $1 OR d.author_id = $1 "
            "ORDER BY d.created_ts DESC, d.id ASC",
        user_id);

    DocumentsListResponse response;
    response.documents = result.AsContainer<std::vector<DocumentItem>>(
        userver::storages::postgres::kRowTag);

    for (auto& doc : response.documents) {
      if (doc.author_photo_url.has_value()) {
        doc.author_photo_url =
            utils::s3_presigned_links::GeneratePhotoPresignedLink(
                doc.author_photo_url.value(), utils::s3_presigned_links::Download);
      }
    }

    // Collect document IDs and employee IDs from chain_metadata
    std::vector<std::string> doc_ids;
    std::unordered_set<std::string> employee_id_set;
    for (const auto& doc : response.documents) {
      doc_ids.push_back(doc.id);
      if (doc.chain_metadata.has_value()) {
        for (const auto& item : doc.chain_metadata.value()) {
          employee_id_set.insert(item.employee_id);
        }
      }
    }

    // Query A: Fetch all signatures for the returned documents
    struct SignatureRow {
      std::string id;
      std::string document_id;
      std::string employee_id;
      std::string signature_path;
      userver::storages::postgres::TimePoint created_ts;
      std::string signature_type;
    };

    // signature key: (document_id, employee_id)
    std::unordered_map<std::string, SignatureRow> sig_map;
    if (!doc_ids.empty()) {
      auto sig_result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT ds.id, ds.document_id, ds.employee_id, ds.signature_path, ds.created_ts, ds.signature_type "
          "FROM working_day_" + company_id + ".document_signatures ds "
          "WHERE ds.document_id = ANY($1)",
          doc_ids);

      for (auto row : sig_result) {
        auto [id, document_id, employee_id, signature_path, created_ts, signature_type] =
            row.As<std::string, std::string, std::string, std::string,
                    userver::storages::postgres::TimePoint, std::string>();
        std::string key = document_id + ":" + employee_id;
        sig_map[key] = {id, document_id, employee_id, signature_path, created_ts, signature_type};
      }
    }

    // Query B: Fetch employee names
    std::unordered_map<std::string, std::string> name_map;
    std::vector<std::string> emp_ids(employee_id_set.begin(), employee_id_set.end());
    if (!emp_ids.empty()) {
      auto emp_result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT id, name, surname, patronymic "
          "FROM working_day_" + company_id + ".employees "
          "WHERE id = ANY($1)",
          emp_ids);

      for (auto row : emp_result) {
        auto [id, name, surname, patronymic] =
            row.As<std::string, std::string, std::string,
                    std::optional<std::string>>();
        std::string full_name = surname + " " + name;
        if (patronymic.has_value() && !patronymic.value().empty()) {
          full_name += " " + patronymic.value();
        }
        name_map[id] = full_name;
      }
    }

    // Post-processing: enrich chain_metadata items
    for (auto& doc : response.documents) {
      if (!doc.chain_metadata.has_value()) continue;
      for (auto& item : doc.chain_metadata.value()) {
        // Set employee_name
        auto name_it = name_map.find(item.employee_id);
        if (name_it != name_map.end()) {
          item.employee_name = name_it->second;
        }
        // Set signature info if exists
        std::string key = doc.id + ":" + item.employee_id;
        auto sig_it = sig_map.find(key);
        if (sig_it != sig_map.end()) {
          item.signature_id = sig_it->second.id;
          item.signature_path = sig_it->second.signature_path;
          auto tp = sig_it->second.created_ts;
          item.signed_at = userver::utils::datetime::Timestring(tp);
          item.signature_type = sig_it->second.signature_type;
        }
      }
    }

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendDocumentsList(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsListHandler>();
}

}  // namespace views::v1::documents::list