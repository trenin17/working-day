#define V1_TRACKER_PROJECTS_LIST

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>

#include "utils/s3_presigned_links.hpp"


namespace views::v1::tracker::projects::list {

namespace {

class TrackerProjectsListHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-projects-list";

    TrackerProjectsListHandler(
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

    const auto& company_id = ctx.GetData<std::string>("company_id");
    const auto& user_id = ctx.GetData<std::string>("user_id");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT  "
        "  p.project_id, p.title, p.image_url, p.creator "
        "FROM working_day_" + company_id + ".tracker_projects p "
        "LEFT JOIN working_day_" + company_id + ".tracker_project_assigned_users au "
        "  ON au.project_id = p.project_id "
        "WHERE p.creator = $1 "
        "   OR au.employee_id = $1 "
        "ORDER BY p.created_ts DESC",
        user_id
    );

    TrackerProjectsListResponse response;
    response.projects = result.AsContainer<std::vector<TrackerProjectsItemResponseShort>>(
        userver::storages::postgres::kRowTag);

    for (auto& project : response.projects) {
      if (project.image_url.has_value()) {
        project.image_url = utils::s3_presigned_links::GenerateTrackerProjectsMediaPresignedLink(
            project.image_url.value(), utils::s3_presigned_links::Download);
      }
    }

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerProjectsList(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerProjectsListHandler>();
}

}  // namespace views::v1::tracker::projects::list