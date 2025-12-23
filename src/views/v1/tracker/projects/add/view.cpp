#define V1_TRACKER_PROJECTS_ADD
#define USERVER_POSTGRES_ENABLE_LEGACY_TIMESTAMP 1

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>


#include "definitions/all.hpp"

#include "core/reverse_index/view.hpp"


namespace views::v1::tracker::projects::add {

namespace {


core::reverse_index::ReverseIndexResponse AddProjectToReverseIndexFunc(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::TrackerProjectsAllData data) {

    std::vector<std::string> words;
    std::istringstream stream(data.title.value());
    std::string word;

    while (stream >> word) {
        words.push_back(core::reverse_index::ConvertToLower(word));
    }

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(data.project_id);

  for (auto& w : words) {
    auto separator = (parameters.Size() == 1 ? "[" : ", ");
    parameters.PushBack(w);
    filter += fmt::format("{}${}", separator, parameters.Size());
  }

  auto result =
      cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "WITH input_data AS ( "
                       "  SELECT ARRAY" +
                           filter +
                           "] AS keys, $1 AS id "
                           ") "
                           "INSERT INTO working_day_" +
                           data.company_id +
                           ".reverse_index (key, ids, entity_type) "
                           "SELECT key, ARRAY[id] AS ids, 'projects' AS entity_type "
                           "FROM input_data, LATERAL unnest(keys) AS key "
                           "ON CONFLICT (key, entity_type) DO UPDATE "
                           "SET ids = array_append(working_day_" +
                           data.company_id +
                           ".reverse_index.ids, "
                           "EXCLUDED.ids[1]); ",
                       parameters);

  core::reverse_index::ReverseIndexResponse response(data.project_id);

  return response;
}


class TrackerProjectsAddHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-projects-add";

  TrackerProjectsAddHandler(
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

    TrackerProjectsItemRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto trx = pg_cluster_->Begin(
        "tracker_projects_add",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    std::string project_id = request_body.title + "-" + userver::utils::generators::GenerateUuid();

    if (request_body.assigned_users_ids) {
      auto check = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT id FROM working_day_" + company_id +
          ".employees WHERE id = ANY($1)",
          *request_body.assigned_users_ids);

      if (check.Size() != request_body.assigned_users_ids->size()) {
        request.GetHttpResponse().SetStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{
            "Invalid assigned_users_ids: one or more employees not found"}
            .ToJsonString();
      }
    }

    auto result = trx.Execute(
        "INSERT INTO working_day_" + company_id +
            ".tracker_projects (project_id, title, description, creator, status) "
            "VALUES ($1, $2, $3, $4, $5)",
        project_id,
        request_body.title,
        request_body.description,
        user_id,
        request_body.status.value_or("Open")
      );

    if (request_body.assigned_users_ids) {
      for (const auto& employee_id : *request_body.assigned_users_ids) {
        trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".tracker_project_assigned_users (project_id, employee_id) "
              "VALUES ($1, $2)  ON CONFLICT DO NOTHING",
          project_id,
          employee_id
        );
      }
    }
    trx.Commit();

    core::reverse_index::TrackerProjectsAllData data{project_id, request_body.title};
    data.company_id = company_id;

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data]() -> core::reverse_index::ReverseIndexResponse {
          return AddProjectToReverseIndexFunc(cluster, data);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    TrackerProjectsAddResponse response;
    response.project_id = project_id;
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerProjectsAdd(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerProjectsAddHandler>();
}

}  // namespace views::v1::tracker::projects::add