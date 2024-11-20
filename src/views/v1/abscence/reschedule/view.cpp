#define USERVER_POSTGRES_ENABLE_LEGACY_TIMESTAMP 1

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>

using json = nlohmann::json;

namespace views::v1::abscence::reschedule {

const std::string tz = "UTC";

namespace {

class AbscenceRescheduleRequest {
 public:
  AbscenceRescheduleRequest(const std::string& body) {
    using namespace userver::utils::datetime;
    auto j = json::parse(body);
    reschedule_date = Stringtime(
        Timestring(Stringtime(j["reschedule_date"], tz, "%Y-%m-%dT%H:%M:%E6S"),
                   tz, "%Y-%m-%d"),
        tz, "%Y-%m-%d");
    action_id = j["action_id"];
  }

  userver::storages::postgres::TimePoint reschedule_date;
  std::string action_id;
};

class AbscenceRescheduleResponse {
 public:
  std::string ToJSON() const {
    nlohmann::json j;
    j["action_id"] = action_id;
    return j.dump();
  }

  std::string action_id;
};

class HeadId {
 public:
  std::optional<std::string> head_id;
};

class UserAction {
 public:
  std::string id, type;
  userver::storages::postgres::TimePoint start_date, end_date;
  std::optional<std::string> status;
  std::string user_id;
};

class ErrorMessage {
 public:
  std::string ToJSON() const {
    json j;
    j["message"] = message;

    return j.dump();
  }

  std::string message;
};

class AbscenceRescheduleHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-abscence-reschedule";

  AbscenceRescheduleHandler(
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

    AbscenceRescheduleRequest request_body(request.RequestBody());
    auto user_id = ctx.GetData<std::string>("user_id");
    auto company_id = ctx.GetData<std::string>("company_id");

    std::string notification_text = "Unknown notification";

    using namespace std::literals::chrono_literals;
    auto trx = pg_cluster_->Begin(
        "reschedule_abscence",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    auto action_to_reschedule =
        trx.Execute(
               "SELECT id, type, start_date, end_date, status, user_id "
               "FROM working_day_" +
                   company_id +
                   ".actions "
                   "WHERE id = $1",
               request_body.action_id)
            .AsSingleRow<UserAction>(userver::storages::postgres::kRowTag);
    auto action_status = action_to_reschedule.status;

    auto new_action_id = userver::utils::generators::GenerateUuid();

    auto result = trx.Execute(
        "INSERT INTO working_day_" + company_id +
            ".actions(id, type, user_id, start_date, "
            "end_date, status, underlying_action_id) "
            "VALUES($1, $2, $3, $4, $5, $6, $7) "
            "ON CONFLICT (id) "
            "DO NOTHING",
        new_action_id, action_to_reschedule.type, user_id,
        request_body.reschedule_date,
        request_body.reschedule_date +
            (action_to_reschedule.end_date - action_to_reschedule.start_date),
        action_status, request_body.action_id);

    result = trx.Execute("UPDATE working_day_" + company_id +
                             ".actions "
                             "SET blocking_actions_ids = $2 "
                             "WHERE id = $1",
                         request_body.action_id, std::vector{new_action_id});

    std::string sender_id = user_id;
    if (user_id == action_to_reschedule.user_id) {
      notification_text =
          "Ваш отпуск с " +
          userver::utils::datetime::Timestring(action_to_reschedule.start_date,
                                               "UTC", "%d.%m.%Y") +
          " был перенесен на " +
          userver::utils::datetime::Timestring(request_body.reschedule_date,
                                               "UTC", "%d.%m.%Y") +
          ".";

      /* TODO
      auto head_id =
          trx.Execute(
                "SELECT head_id "
                "FROM working_day_" + company_id + ".employees "
                "WHERE id = $1",
                user_id)
              .AsSingleRow<HeadId>(userver::storages::postgres::kRowTag)
              .head_id;

      if (request_body.type == "vacation") {
        notification_text =
            "Ваш сотрудник запросил отпуск c " +
            userver::utils::datetime::Timestring(request_body.start_date, tz,
      "%d.%m.%Y") + " по " +
      userver::utils::datetime::Timestring(request_body.end_date, tz,
      "%d.%m.%Y") +
            ". Подтвердите или отклоните его.";
      }

      if (request_body.type == "vacation") {
        auto notification_id = userver::utils::generators::GenerateUuid();
        result = trx.Execute(
            "INSERT INTO working_day_" + company_id + ".notifications(id, type,
      text, user_id, " "sender_id, action_id) " "VALUES($1, $2, $3, $4, $5, $6)
      " "ON CONFLICT (id) " "DO NOTHING", notification_id, request_body.type +
      "_request", notification_text, head_id.value_or(user_id), user_id,
      action_id);
      } */
    } else {
      auto head_id =
          trx.Execute(
                 "SELECT head_id "
                 "FROM working_day_" +
                     company_id +
                     ".employees "
                     "WHERE id = $1",
                 user_id)
              .AsSingleRow<HeadId>(userver::storages::postgres::kRowTag)
              .head_id;
      if (head_id.has_value()) {
        sender_id = head_id.value();
      }
      notification_text =
          "Вам предложили изменить дату отпуска с " +
          userver::utils::datetime::Timestring(action_to_reschedule.start_date,
                                               "UTC", "%d.%m.%Y") +
          " на " +
          userver::utils::datetime::Timestring(request_body.reschedule_date,
                                               "UTC", "%d.%m.%Y") +
          ".";
    }
    auto notification_id = userver::utils::generators::GenerateUuid();
    result = trx.Execute(
        "INSERT INTO working_day_" + company_id +
            ".notifications(id, type, text, user_id, "
            "sender_id, action_id) "
            "VALUES($1, $2, $3, $4, $5, $6) "
            "ON CONFLICT (id) "
            "DO NOTHING",
        notification_id, action_to_reschedule.type + "_reschedule",
        notification_text, user_id, sender_id, request_body.action_id);

    trx.Commit();

    AbscenceRescheduleResponse response{new_action_id};
    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAbscenceReschedule(
    userver::components::ComponentList& component_list) {
  component_list.Append<AbscenceRescheduleHandler>();
}

}  // namespace views::v1::abscence::reschedule
