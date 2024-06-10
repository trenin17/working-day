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

namespace views::v1::abscence::request {

const std::string tz = "UTC";

namespace {

class AbscenceRequestRequest {
 public:
  AbscenceRequestRequest(const std::string& body) {
    using namespace userver::utils::datetime;
    using namespace std::literals::chrono_literals;
    auto j = json::parse(body);
    start_date = Stringtime(
        Timestring(Stringtime(j["start_date"], tz, "%Y-%m-%dT%H:%M:%E6S"), tz,
                   "%Y-%m-%d"),
        tz, "%Y-%m-%d");
    end_date = Stringtime(
        Timestring(Stringtime(j["end_date"], tz, "%Y-%m-%dT%H:%M:%E6S"), tz,
                   "%Y-%m-%d"),
        tz, "%Y-%m-%d");
    end_date += 1439min;  // + 23:59
    type = j["type"];
  }

  userver::storages::postgres::TimePoint start_date, end_date;
  std::string type;
};

class AbscenceRequestResponse {
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

class AbscenceRequestHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-abscence-request";

  AbscenceRequestHandler(
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

    AbscenceRequestRequest request_body(request.RequestBody());
    auto user_id = ctx.GetData<std::string>("user_id");
    auto company_id = ctx.GetData<std::string>("company_id");

    auto action_id = userver::utils::generators::GenerateUuid();

    std::optional<std::string> action_status;
    std::string notification_text = "Unknown notification";
    if (request_body.type == "vacation") {
      action_status = "pending";
      notification_text = "Ваш сотрудник запросил отпуск c " +
                          userver::utils::datetime::Timestring(
                              request_body.start_date, tz, "%d.%m.%Y") +
                          " по " +
                          userver::utils::datetime::Timestring(
                              request_body.end_date, tz, "%d.%m.%Y") +
                          ". Подтвердите или отклоните его.";
    }

    auto trx = pg_cluster_->Begin(
        "request_abscence",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    auto result = trx.Execute("INSERT INTO working_day_" + company_id +
                                  ".actions(id, type, user_id, start_date, "
                                  "end_date, status) "
                                  "VALUES($1, $2, $3, $4, $5, $6) "
                                  "ON CONFLICT (id) "
                                  "DO NOTHING",
                              action_id, request_body.type, user_id,
                              request_body.start_date, request_body.end_date,
                              action_status);

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

    if (request_body.type == "vacation") {
      auto notification_id = userver::utils::generators::GenerateUuid();
      result = trx.Execute("INSERT INTO working_day_" + company_id +
                               ".notifications(id, type, text, user_id, "
                               "sender_id, action_id) "
                               "VALUES($1, $2, $3, $4, $5, $6) "
                               "ON CONFLICT (id) "
                               "DO NOTHING",
                           notification_id, request_body.type + "_request",
                           notification_text, head_id.value_or(user_id),
                           user_id, action_id);
    }

    trx.Commit();

    AbscenceRequestResponse response{action_id};
    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAbscenceRequest(userver::components::ComponentList& component_list) {
  component_list.Append<AbscenceRequestHandler>();
}

}  // namespace views::v1::abscence::request