#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>

using json = nlohmann::json;

namespace views::v1::abscence::verdict {

namespace {

class AbscenceVerdictRequest {
 public:
  AbscenceVerdictRequest(const std::string& body) {
    auto j = json::parse(body);
    action_id = j["action_id"];
    approve = j["approve"].get<bool>();
  }

  std::string action_id;
  bool approve;
};

class ActionInfo {
 public:
  std::string employee_id, type;
};

class AbscenceVerdictHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-abscence-verdict";

  AbscenceVerdictHandler(
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
    AbscenceVerdictRequest request_body(request.RequestBody());
    auto user_id = ctx.GetData<std::string>("user_id");
    std::string action_status = "denied";
    std::string notification_text = "Ваш запрос на отпуск был отклонен.";
    if (request_body.approve) {
      action_status = "approved";
      notification_text = "Ваш запрос на отпуск был одобрен.";
    }

    auto trx = pg_cluster_->Begin(
        "verdict_abscence",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    auto action_info =
        trx.Execute(
               "SELECT user_id, type "
               "FROM working_day.actions "
               "WHERE id = $1",
               request_body.action_id)
            .AsSingleRow<ActionInfo>(userver::storages::postgres::kRowTag);

    auto result = trx.Execute(
        "UPDATE working_day.actions "
        "SET status = $2"
        "WHERE id = $1 ",
        request_body.action_id, action_status);

    auto notification_id = userver::utils::generators::GenerateUuid();
    result = trx.Execute(
        "INSERT INTO working_day.notifications(id, type, text, user_id, "
        "sender_id) "
        "VALUES($1, $2, $3, $4, $5) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        notification_id, action_info.type + "_" + action_status,
        notification_text, action_info.employee_id, user_id);

    trx.Commit();

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAbscenceVerdict(userver::components::ComponentList& component_list) {
  component_list.Append<AbscenceVerdictHandler>();
}

}  // namespace views::v1::abscence::verdict