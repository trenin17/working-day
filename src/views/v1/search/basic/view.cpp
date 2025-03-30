#define V1_SEARCH_BASIC

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>

#include "core/reverse_index/view.hpp"
#include "definitions/all.hpp"
#include "utils/s3_presigned_links.hpp"

namespace views::v1::search_basic {

namespace {

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
    std::string entity_type;
  };

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& company_id = ctx.GetData<std::string>("company_id");

    SearchBasicRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    request_body.search_key =
        core::reverse_index::ConvertToLower(request_body.search_key);


    std::string tag = request_body.tag.value_or("employees");
    std::string entity_filter = (tag == "all") 
                                ? "" 
                                : "AND entity_type = ";
    if (tag == "employees") {
      entity_filter += "'employees'";
    } else if (tag == "tasks") {
      entity_filter += "'tasks'";
    }
    
    auto result_ids = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT ids, entity_type "
        "FROM working_day_" +
            company_id +
            ".reverse_index "
            "WHERE key = $1 " +
            entity_filter + ";",
        request_body.search_key);

    auto IDs = result_ids.AsOptionalSingleRow<IDsRow>(
        userver::storages::postgres::kRowTag);

    SearchResponse response;

    if (IDs.has_value()) {
      userver::storages::postgres::ParameterStore parameters;
      std::string filter;

      auto append = [&](const auto& value) {
        auto separator = (parameters.IsEmpty() ? "(" : ", ");
        parameters.PushBack(value);
        filter += fmt::format("{}${}", separator, parameters.Size());
      };

      for (auto& val : IDs.value().ids) {
        append(val);
      }

      if (parameters.Size() != 0 && IDs.value().entity_type == "employees") {
        auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT id, name, surname, patronymic, photo_link "
            "FROM working_day_" +
                company_id +
                ".employees "
                "WHERE id IN " +
                filter + ");",
            parameters);

        response.employees = result.AsContainer<std::vector<ListEmployee>>(
            userver::storages::postgres::kRowTag);
      } else if (parameters.Size() != 0 && IDs.value().entity_type == "tasks") {
        auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kSlave,
          "SELECT title, project_name, id, creator, assignee "
          "FROM working_day_" +
              company_id +
              ".tracker_tasks "
              "WHERE id IN " +
              filter + ");",
          parameters);

      response.tasks = result.AsContainer<std::vector<TrackerTasksListItem>>(
          userver::storages::postgres::kRowTag);
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

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendSearchBasic(userver::components::ComponentList& component_list) {
  component_list.Append<SearchBasicHandler>();
}

}  // namespace views::v1::search_basic