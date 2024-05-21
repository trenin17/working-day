#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

using json = nlohmann::json;

namespace views::v1::payments {

namespace {

class Payment {
 public:
  json ToJSONObject() const {
    json j;
    j["id"] = id;
    j["user_id"] = user_id;
    j["amount"] = amount;
    j["payroll_date"] = userver::utils::datetime::Timestring(
        payroll_date, "UTC", "%Y-%m-%dT%H:%M:%E6S");

    return j;
  }

  std::string id, user_id;
  double amount;
  userver::storages::postgres::TimePoint payroll_date;
};

class PaymentsResponse {
 public:
  std::string ToJSON() const {
    json j;
    j["payments"] = json::array();
    for (const auto& payment : payments) {
      j["payments"].push_back(payment.ToJSONObject());
    }
    return j.dump();
  }

  std::vector<Payment> payments;
};

class PaymentsHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-payments";

  PaymentsHandler(
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

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, user_id, amount, payroll_date "
        "FROM working_day_" + company_id + ".payments "
        "WHERE user_id = $1 "
        "LIMIT 100",
        user_id);

    PaymentsResponse response{result.AsContainer<std::vector<Payment>>(
        userver::storages::postgres::kRowTag)};

    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendPayments(userver::components::ComponentList& component_list) {
  component_list.Append<PaymentsHandler>();
}

}  // namespace views::v1::payments