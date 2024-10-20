#define V1_ABSCENCE_REQUEST

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

#include <fmt/core.h>

using json = nlohmann::json;

namespace views::v1::abscence::request {

const std::string tz = "UTC";

namespace {

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

    AbscenceRequestRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());
    auto user_id = ctx.GetData<std::string>("user_id");
    auto company_id = ctx.GetData<std::string>("company_id");

    auto action_id = userver::utils::generators::GenerateUuid();

    std::optional<std::string> action_status;
    action_status = "pending";
    std::string notification_text;
    constexpr auto notification_fmt =
        "Ваш сотрудник запросил {} c {} по {}. Подтвердите или отклоните "
        "запрос.";
    auto start_date = userver::utils::datetime::Timestring(
        request_body.start_date, tz, "%Y-%m-%d");
    auto end_date = userver::utils::datetime::Timestring(request_body.end_date,
                                                         tz, "%Y-%m-%d");
    auto start_date_time = userver::utils::datetime::Timestring(
        request_body.start_date, tz, "%Y-%m-%d %H:%M:%S");
    auto end_date_time = userver::utils::datetime::Timestring(
        request_body.end_date, tz, "%Y-%m-%d %H:%M:%S");

    if (request_body.type == "vacation") {
      notification_text =
          fmt::format(notification_fmt, "отпуск", start_date, end_date);
    } else if (request_body.type == "sick_leave") {
      notification_text =
          fmt::format(notification_fmt, "больничный", start_date, end_date);
    } else if (request_body.type == "unpaid_vacation") {
      notification_text = fmt::format(notification_fmt, "неоплачиваемый отпуск",
                                      start_date, end_date);
    } else if (request_body.type == "business_trip") {
      notification_text =
          fmt::format(notification_fmt, "командировку", start_date, end_date);
    } else if (request_body.type == "overtime") {
      notification_text = fmt::format(notification_fmt, "сверхурочные",
                                      start_date_time, end_date_time);
    } else {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Unknown abscence type"}.ToJsonString();
    }

    if (request_body.type != "overtime") {
      using namespace userver::utils::datetime;
      using namespace std::literals::chrono_literals;
      request_body.start_date = Stringtime(
          Timestring(request_body.start_date, tz, "%Y-%m-%d"), tz, "%Y-%m-%d");
      request_body.end_date =
          Stringtime(Timestring(request_body.end_date, tz, "%Y-%m-%d"), tz,
                     "%Y-%m-%d") +
          1439min;  // + 23:59
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

    auto notification_id = userver::utils::generators::GenerateUuid();
    result = trx.Execute("INSERT INTO working_day_" + company_id +
                             ".notifications(id, type, text, user_id, "
                             "sender_id, action_id) "
                             "VALUES($1, $2, $3, $4, $5, $6) "
                             "ON CONFLICT (id) "
                             "DO NOTHING",
                         notification_id, "vacation_request", notification_text,
                         head_id.value_or(user_id), user_id, action_id);

    trx.Commit();

    AbscenceRequestResponse response;
    response.action_id = action_id;
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAbscenceRequest(userver::components::ComponentList& component_list) {
  component_list.Append<AbscenceRequestHandler>();
}

}  // namespace views::v1::abscence::request