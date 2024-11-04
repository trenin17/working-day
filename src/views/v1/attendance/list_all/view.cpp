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

const std::string tz = "UTC";

struct Team {
  std::string id;
};

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
        "SELECT et.team_id "
        "FROM working_day_" +
            company_id +
            ".employee_team et "
            "WHERE et.employee_id = $1",
        user_id);

    auto teams = result.AsContainer<std::vector<Team>>(
        userver::storages::postgres::kRowTag);

    userver::storages::postgres::ParameterStore parameters;
    std::string filter;

    parameters.PushBack(request_body.from);
    parameters.PushBack(request_body.to);
    for (const auto& team : teams) {
      parameters.PushBack(team.id);
      filter += "$" + std::to_string(parameters.Size()) + ",";
    }
    filter.pop_back();

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT a.start_date, a.end_date, NULL::TEXT as type,"
        "ROW"
        "(e.id, e.name, "
        "e.surname, e.patronymic, "
        "NULL::text, e.subcompany) "
        "FROM working_day_" +
            company_id +
            ".employees e "
            "LEFT JOIN working_day_" +
            company_id +
            ".actions a "
            "ON e.id = a.user_id AND a.start_date >= $1 AND a.start_date "
            "<= $2 AND a.type = 'attendance' "
            "JOIN working_day_" +
            company_id +
            ".employee_team et "
            "ON e.id = et.employee_id "
            "WHERE et.team_id IN (" +
            filter + ")",
        parameters);

    auto attendances = result.AsContainer<std::vector<AttendanceListItem>>(
        userver::storages::postgres::kRowTag);

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT a.start_date, a.end_date, a.type, "
        "ROW"
        "(e.id, e.name, "
        "e.surname, e.patronymic, "
        "NULL::text, e.subcompany) "
        "FROM working_day_" +
            company_id +
            ".employees e "
            "JOIN working_day_" +
            company_id +
            ".actions a "
            "ON e.id = a.user_id AND ((a.start_date >= $1 AND a.start_date "
            "<= $2) OR (a.end_date >= $1 AND a.end_date <= $2) OR "
            "(a.start_date < $1 AND a.end_date > $2))"
            " AND a.type <> 'attendance' AND a.type <> 'overtime' AND a.status "
            "= 'approved'"
            "JOIN working_day_" +
            company_id +
            ".employee_team et "
            "ON e.id = et.employee_id "
            "WHERE et.team_id IN (" +
            filter + ")",
        parameters);

    auto other_actions = result.AsContainer<std::vector<AttendanceListItem>>(
        userver::storages::postgres::kRowTag);

    std::set<std::pair<std::string, std::string>> other_actions_days;
    for (const auto& action : other_actions) {
      if (!action.start_date.has_value()) {
        continue;
      }
      auto cur_date = action.start_date.value();

      using namespace userver::utils::datetime;
      using namespace std::literals::chrono_literals;
      for (; cur_date < action.end_date.value(); cur_date += 1440min) {
        other_actions_days.insert(
            {Timestring(cur_date, tz, "%Y-%m-%d"), action.employee.id});
      }
    }

    AttendanceListAllResponse response;

    for (auto& attendance : attendances) {
      if (attendance.start_date.has_value()) {
        auto date = userver::utils::datetime::Timestring(
            attendance.start_date.value(), tz, "%Y-%m-%d");
        if (other_actions_days.find({date, attendance.employee.id}) !=
            other_actions_days.end()) {
          continue;
        }
      }
      response.attendances.push_back(std::move(attendance));
    }

    for (const auto& action : other_actions) {
      if (!action.start_date.has_value()) {
        continue;
      }

      auto cur_date = action.start_date.value();

      using namespace std::literals::chrono_literals;
      for (; cur_date < action.end_date.value(); cur_date += 1440min) {
        if (!(cur_date >= request_body.from && cur_date <= request_body.to)) {
          continue;
        }
        AttendanceListItem item;
        item.start_date = cur_date;
        item.end_date = cur_date + 1439min;
        item.abscence_type = action.abscence_type;
        item.employee = action.employee;
        response.attendances.push_back(std::move(item));
      }
    }

    result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT a.start_date, a.end_date, a.type, "
        "ROW"
        "(e.id, e.name, "
        "e.surname, e.patronymic, "
        "NULL::text, e.subcompany) "
        "FROM working_day_" +
            company_id +
            ".employees e "
            "JOIN working_day_" +
            company_id +
            ".actions a "
            "ON e.id = a.user_id AND a.start_date >= $1 AND a.start_date "
            "<= $2 AND a.type = 'overtime' AND a.status = 'approved'"
            "JOIN working_day_" +
            company_id +
            ".employee_team et "
            "ON e.id = et.employee_id "
            "WHERE et.team_id IN (" +
            filter + ")",
        parameters);

    auto overtimes = result.AsContainer<std::vector<AttendanceListItem>>(
        userver::storages::postgres::kRowTag);

    for (auto& overtime : overtimes) {
      response.attendances.push_back(std::move(overtime));
    }

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