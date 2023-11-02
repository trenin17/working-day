#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

using json = nlohmann::json;

namespace views::v1::payments::add_bulk {

namespace {

class Payment {
 public:
  Payment(const json& body) {
    id = body["id"];
    user_id = body["user_id"];
    amount = body["amount"];
    payroll_date = userver::utils::datetime::Stringtime(
        body["payroll_date"], "UTC",
        "%Y-%m-%dT%H:%M:%E6S");
  }
 
  std::string id, user_id;
  double amount;
  userver::storages::postgres::TimePoint payroll_date;
};

class PaymentsAddBulkRequest {
 public:
  PaymentsAddBulkRequest(const std::string& body) {
    auto j = json::parse(body);
    for (const auto& p: j["payments"]) {
      Payment payment(p);

      ids.push_back(payment.id);
      user_ids.push_back(payment.user_id);
      amounts.push_back(payment.amount);
      payroll_dates.push_back(payment.payroll_date);

      payments.push_back(std::move(payment));
    }
  }

  std::vector<Payment> payments;
  std::vector<std::string> ids, user_ids;
  std::vector<double> amounts;
  std::vector<userver::storages::postgres::TimePoint> payroll_dates;
};

class PaymentsAddBulkHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-payments-add-bulk";

  PaymentsAddBulkHandler(
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
    PaymentsAddBulkRequest request_body(request.RequestBody());

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.payments(id, user_id, amount, payroll_date) "
        "VALUES (UNNEST($1), UNNEST($2), UNNEST($3), UNNEST($4)) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        request_body.ids, request_body.user_ids, request_body.amounts, request_body.payroll_dates);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendPaymentsAddBulk(userver::components::ComponentList& component_list) {
  component_list.Append<PaymentsAddBulkHandler>();
}

}  // namespace views::v1::payments::add_bulk