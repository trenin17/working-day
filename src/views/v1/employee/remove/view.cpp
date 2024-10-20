#define V1_REMOVE_EMPLOYEE

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>

#include "core/json_compatible/struct.hpp"
#include "core/reverse_index/view.hpp"

#include "definitions/all.hpp"

namespace views::v1::employee::remove {

namespace {

struct AllValuesRow {
  std::string name, surname, role;
  std::optional<std::string> patronymic;
  std::vector<std::string> phones;
  std::optional<std::string> email, birthday, telegram_id, vk_id, team;
};

core::reverse_index::ReverseIndexResponse DeleteReverseIndexFunc(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::EmployeeAllData data) {
  auto result =
      cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "SELECT name, surname, role, patronymic, phones, "
                       "email, birthday, telegram_id, vk_id, team "
                       "FROM working_day_" +
                           data.company_id.value() +
                           ".employees "
                           "WHERE id = $1;",
                       data.employee_id);

  auto values_res =
      result.AsSingleRow<AllValuesRow>(userver::storages::postgres::kRowTag);

  std::vector<std::optional<std::string>> old_values = {
      values_res.name,        values_res.surname, values_res.role,
      values_res.patronymic,  values_res.email,   values_res.birthday,
      values_res.telegram_id, values_res.vk_id,   values_res.team};

  old_values.insert(old_values.end(), values_res.phones.begin(),
                    values_res.phones.end());

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(data.employee_id);

  for (const auto& field : old_values) {
    if (field.has_value()) {
      auto separator = (parameters.Size() == 1 ? "(" : ", ");
      parameters.PushBack(field.value());
      filter += fmt::format("{}${}", separator, parameters.Size());
    }
  }

  auto result2 =
      cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "UPDATE working_day_" + data.company_id.value() +
                           ".reverse_index "
                           "SET ids = array_remove(ids, $1) "
                           "WHERE key IN " +
                           filter +
                           "); "
                           "DELETE FROM working_day_" +
                           data.company_id.value() +
                           ".reverse_index "
                           "WHERE key IN " +
                           filter + ") AND ids = '{}'; ",
                       parameters);

  core::reverse_index::ReverseIndexResponse response(data.employee_id);
  return response;
}

class RemoveEmployeeHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-remove";

  RemoveEmployeeHandler(
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

    const auto& employee_id = request.GetArg("employee_id");
    const auto company_id = ctx.GetData<std::string>("company_id");

    if (employee_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      ErrorMessage err_msg("Unauthorized");
      return err_msg.ToJsonString();
    }

    core::reverse_index::EmployeeAllData data{employee_id};
    data.company_id = company_id;

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data]() -> core::reverse_index::ReverseIndexResponse {
          return DeleteReverseIndexFunc(cluster, data);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "DELETE FROM working_day_" + company_id +
            ".employees "
            "WHERE id = $1",
        employee_id);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendRemoveEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<RemoveEmployeeHandler>();
}

}  // namespace views::v1::employee::remove