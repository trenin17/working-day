#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

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

class AddHeadEmployeeHandler final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-add-head";

  AddHeadEmployeeHandler(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}
  
  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext&) const override {
    const auto& user_id = request.GetHeader("user_id");
    const auto& employee_id = request.GetArg("employee_id");

    if (user_id.empty() || employee_id.empty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kUnauthorized);
      return "Unauthorized";
    }

    AddHeadEmployeeRequest request_body(employee_id, request.RequestBody());

    auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "UPDATE working_day.employees "
          "SET head_id = $2 "
          "WHERE id = $1",
          request_body.employee_id, request_body.head_id);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

} // namespace

void AppendAddHeadEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<AddHeadEmployeeHandler>();
}

} // namespace views::v1::employee::add_head