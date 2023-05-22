#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>

using json = nlohmann::json;

namespace views::v1::attendance::add {

namespace {

class AttendanceAddRequest {
 public:
  AttendanceAddRequest(const std::string& body) {
    auto j = json::parse(body);
    start_date = userver::utils::datetime::Stringtime(j["start_date"], "UTC", "%Y-%m-%dT%H:%M:%E6S");
    end_date = userver::utils::datetime::Stringtime(j["end_date"], "UTC", "%Y-%m-%dT%H:%M:%E6S");
  }

  userver::storages::postgres::TimePoint start_date, end_date;
};

class AttendanceAddResponse {
 public:

  std::string ToJSON() const {
    nlohmann::json j;
    j["action_id"] = action_id;
    return j.dump();
  }

  std::string action_id;
};

class AttendanceAddHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-attendance-add";

  AttendanceAddHandler(
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

    AttendanceAddRequest request_body(request.RequestBody());
    auto employee_id = request.GetArg("employee_id");
    auto user_id = ctx.GetData<std::string>("user_id");

    std::string notification_text = "Вам добавлено новое посещение";

    auto trx = pg_cluster_->Begin("add_attendance",
                         userver::storages::postgres::ClusterHostType::kMaster, {});

    auto action_id = userver::utils::generators::GenerateUuid();
    auto result = trx.Execute(
        "INSERT INTO working_day.actions(id, type, user_id, start_date, end_date) "
        "VALUES($1, $2, $3, $4, $5) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        action_id, "attendance", employee_id, request_body.start_date, request_body.end_date);
      
    auto notification_id = userver::utils::generators::GenerateUuid();
    result = trx.Execute(
        "INSERT INTO working_day.notifications(id, type, text, user_id, sender_id) "
        "VALUES($1, $2, $3, $4, $5) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        notification_id, "attendance_added", notification_text, employee_id, user_id);

    trx.Commit();

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAttendanceAdd(userver::components::ComponentList& component_list) {
  component_list.Append<AttendanceAddHandler>();
}

}  // namespace views::v1::attendance::add