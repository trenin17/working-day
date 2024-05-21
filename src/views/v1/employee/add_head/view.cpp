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

namespace views::v1::employee::add_head {

namespace {

class AddHeadEmployeeRequest {
 public:
  AddHeadEmployeeRequest(const std::string& employee, const std::string& body) {
    employee_id = employee;
    auto j = json::parse(body);
    head_id = j["head_id"];
  }

  std::string employee_id, head_id;
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

class AddHeadEmployeeHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-add-head";

  AddHeadEmployeeHandler(
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
    const auto& employee_id = request.GetArg("employee_id");

    if (employee_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      return ErrorMessage{"Unauthorized"}.ToJSON();
    }

    AddHeadEmployeeRequest request_body(employee_id, request.RequestBody());

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id + ".employees "
        "SET head_id = $2 "
        "WHERE id = $1",
        request_body.employee_id, request_body.head_id);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAddHeadEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<AddHeadEmployeeHandler>();
}

}  // namespace views::v1::employee::add_head