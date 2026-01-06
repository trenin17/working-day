#define V1_EMPLOYEE_PERMISSIONS_LIST

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>

namespace views::v1::employee::permissions::list {

namespace {

class EmployeePermissionsListHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-permissions-list";

    EmployeePermissionsListHandler(
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
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Missing employee_id"}.ToJsonString();
    }

    auto emp_check = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT 1 FROM working_day_" + 
            company_id +
            ".employees "
        "WHERE id = $1",
        employee_id);

    if (emp_check.IsEmpty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"User not found"}.ToJsonString();
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT permission_type, permission_value "
        "FROM working_day_" + 
            company_id + 
            ".employee_permissions "
        "WHERE employee_id = $1",
        employee_id);

    EmployeePermissions response;
    response.permissions = result.AsContainer<std::vector<EmployeePermissionsItem>>(
        userver::storages::postgres::kRowTag);

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendEmployeePermissionsList(userver::components::ComponentList& component_list) {
  component_list.Append<EmployeePermissionsListHandler>();
}

}  // namespace views::v1::employee::permissions::list