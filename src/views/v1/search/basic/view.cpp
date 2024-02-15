#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>

#include "core/json_compatible/struct.hpp"

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::search_basic {

namespace {

class ListEmployee {
 public:
  json ToJSONObject() const {
    json j;
    j["id"] = id;
    j["name"] = name;
    j["surname"] = surname;
    if (patronymic) {
      j["patronymic"] = patronymic.value();
    }
    if (photo_link) {
      j["photo_link"] = photo_link.value();
    }

    return j;
  }

  std::string id, name, surname;
  std::optional<std::string> patronymic, photo_link;
};

struct SearchBasicRequest: public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
};

class SearchBasicResponse {
 public:
  std::string ToJSON() const {
    json j;
    j["employees"] = json::array();
    for (const auto& employee : employees) {
      j["employees"].push_back(employee.ToJSONObject());
    }
    return j.dump();
  }

  std::vector<ListEmployee> employees;
};

class SearchBasicHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-search-basic";

  SearchBasicHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  struct IDsRow {
    std::vector<std::string> ids;
  };

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    SearchBasicRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto result_ids = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT ids "
        "FROM working_day.reverse_index "
        "WHERE key = $1;",
        request_body.search_key);

    auto IDs = result_ids.AsOptionalSingleRow<IDsRow>(userver::storages::postgres::kRowTag);

    SearchBasicResponse response;
    
    // TODO сделать одним запросом WHERE ID in set
    if (IDs.has_value()) {
      for (const auto& employee_id : IDs.value().ids) {
        auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT id, name, surname, patronymic, photo_link "
            "FROM working_day.employees "
            "WHERE id = $1",
            employee_id);

        response.employees.push_back(result.AsSingleRow<ListEmployee>(
            userver::storages::postgres::kRowTag));
      }
    }

    for (auto& employee : response.employees) {
      if (employee.photo_link.has_value()) {
        employee.photo_link =
            utils::s3_presigned_links::GeneratePhotoPresignedLink(
                employee.photo_link.value(),
                utils::s3_presigned_links::Download);
      }
    }
    
    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendSearchBasic(userver::components::ComponentList& component_list) {
  component_list.Append<SearchBasicHandler>();
}

}  // namespace views::v1::search_basic