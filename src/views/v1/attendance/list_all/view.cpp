#define V1_ATTENDANCE_LIST_ALL

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "definitions/all.hpp"

namespace views::v1::attendance::list_all {

namespace {

class AttendanceListAllHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-attendance-list-all";

  AttendanceListAllHandler(
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
    AttendanceListAllRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT working_day_" + company_id + ".actions.start_date, working_day_" + company_id + ".actions.end_date, "
        "ROW"
        "(working_day_" + company_id + ".employees.id, working_day_" + company_id + ".employees.name, "
        "working_day_" + company_id + ".employees.surname, working_day_" + company_id + ".employees.patronymic, "
        "NULL::text) "
        "FROM working_day_" + company_id + ".employees "
        "LEFT JOIN working_day_" + company_id + ".actions "
        "ON working_day_" + company_id + ".employees.id = working_day_" + company_id + ".actions.user_id AND "
        "working_day_" + company_id + ".actions.start_date >= $2 AND working_day_" + company_id + ".actions.end_date "
        "<= $3 AND working_day_" + company_id + ".actions.type = 'attendance' "
        "WHERE working_day_" + company_id + ".employees.id <> $1",
        user_id, request_body.from, request_body.to);

    AttendanceListAllResponse response;
    response.attendances = result.AsContainer<std::vector<AttendanceListItem>>(
        userver::storages::postgres::kRowTag);
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAttendanceListAll(
    userver::components::ComponentList& component_list) {
  component_list.Append<AttendanceListAllHandler>();
}

}  // namespace views::v1::attendance::list_all