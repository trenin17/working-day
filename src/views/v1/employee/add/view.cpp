#define V1_ADD_EMPLOYEE

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "definitions/all.hpp"

#include "core/json_compatible/struct.hpp"
#include "core/reverse_index/view.hpp"

namespace views::v1::employee::add {

namespace {

core::reverse_index::ReverseIndexResponse AddReverseIndexFunc(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::EmployeeAllData data) {
  std::vector<std::optional<std::string>> fields = {
      data.name,     data.surname,     data.patronymic, data.role, data.email,
      data.birthday, data.telegram_id, data.vk_id,      data.team};

  if (data.phones.has_value()) {
    fields.insert(fields.end(), data.phones.value().begin(),
                  data.phones.value().end());
  }

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(data.employee_id);

  for (auto& field : fields) {
    if (field.has_value()) {
      auto separator = (parameters.Size() == 1 ? "[" : ", ");
      transform(field.value().begin(), field.value().end(),
                field.value().begin(), ::tolower);
      parameters.PushBack(field.value());
      filter += fmt::format("{}${}", separator, parameters.Size());
    }
  }

  auto result = cluster->Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      "WITH input_data AS ( "
      "  SELECT ARRAY" +
          filter +
          "] AS keys, $1 AS id "
          ") "
          "INSERT INTO working_day.reverse_index (key, ids) "
          "SELECT key, ARRAY[id] AS ids "
          "FROM input_data, LATERAL unnest(keys) AS key "
          "ON CONFLICT (key) DO UPDATE "
          "SET ids = array_append(working_day.reverse_index.ids, "
          "EXCLUDED.ids[1]); ",
      parameters);

  core::reverse_index::ReverseIndexResponse response(data.employee_id);

  return response;
}

class AddEmployeeHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-add";

  AddEmployeeHandler(
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

    AddEmployeeRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());
    auto company_id = ctx.GetData<std::string>("company_id");

    auto id = userver::utils::generators::GenerateUuid();
    auto password = userver::utils::generators::GenerateUuid();

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.employees(id, name, surname, patronymic, "
        "password, role, company_id) VALUES($1, $2, $3, $4, $5, $6, $7) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        id, request_body.name, request_body.surname, request_body.patronymic,
        password, request_body.role, company_id);

    core::reverse_index::EmployeeAllData data{
        id, request_body.name, request_body.surname, request_body.patronymic,
        request_body.role};

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data]() -> core::reverse_index::ReverseIndexResponse {
          return AddReverseIndexFunc(cluster, data);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    AddEmployeeResponse response(id, password);
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAddEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<AddEmployeeHandler>();
}

}  // namespace views::v1::employee::add