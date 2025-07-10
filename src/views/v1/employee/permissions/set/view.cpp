#define V1_EMPLOYEE_PERMISSIONS_SET

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

namespace views::v1::employee::permissions::set {

namespace {

class EmployeePermissionsSetHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-permissions-set";

    EmployeePermissionsSetHandler(
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
    const auto& employee_id = request.GetArg("employee_id");

    if (employee_id.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{"Missing employee_id"}.ToJsonString();
    }

    EmployeePermissions body;
    body.ParseRegisteredFields(request.RequestBody());

    auto emp_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT 1 FROM working_day_" + 
            company_id +
            ".employees "
        "WHERE id = $1",
        employee_id);
    if (emp_result.IsEmpty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"User not found"}.ToJsonString();
    }

    for (const auto& permission : body.permissions) {
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO working_day_" + company_id + ".employee_permissions "
            "(employee_id, permission_type, permission_value) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (employee_id, permission_type) DO UPDATE "
            "SET permission_value = EXCLUDED.permission_value",
            employee_id, permission.permission_type, permission.permission_value);
    }

    SendNotifications(company_id, user_id, employee_id);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT permission_type, permission_value "
        "FROM working_day_" + company_id + ".employee_permissions "
        "WHERE employee_id = $1",
        employee_id);

    EmployeePermissions response;
    response.permissions = result.AsContainer<std::vector<EmployeePermissionsItem>>(
        userver::storages::postgres::kRowTag);

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;

  struct Employee {
    std::string name, surname;
  };

  void SendNotifications(
    const std::string& company_id,
    const std::string& user_id,
    const std::string& employee_id) const {

        auto emp_result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT name, surname FROM working_day_" + 
                company_id +
                ".employees "
            "WHERE id = $1",
            user_id);

        std::string employee_name;
        if (!emp_result.IsEmpty()) {
            auto emp_row = emp_result.AsSingleRow<Employee>(userver::storages::postgres::kRowTag);
            employee_name = emp_row.name + " " + emp_row.surname;
        }

        std::string notification_text;
        notification_text = fmt::format("Ваши права были изменены пользователем {}.", employee_name);

        userver::storages::postgres::ParameterStore parameters;
        std::string filter;

        parameters.PushBack("generic");
        parameters.PushBack(notification_text);
        parameters.PushBack(userver::utils::generators::GenerateUuid());
        parameters.PushBack(employee_id);

        filter = "($3, $1, $2, $4)";
        
        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO working_day_" + company_id + 
            ".notifications(id, type, text, user_id) "
            "VALUES " + filter + " ON CONFLICT (id) DO NOTHING",
            parameters);
    }
};

}  // namespace

void AppendEmployeePermissionsSet(userver::components::ComponentList& component_list) {
  component_list.Append<EmployeePermissionsSetHandler>();
}

}  // namespace views::v1::employee::permissions::set