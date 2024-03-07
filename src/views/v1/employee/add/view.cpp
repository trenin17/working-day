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

#include "core/json_compatible/struct.hpp"
#include "core/reverse_index/view.hpp"

namespace views::v1::employee::add {

namespace {

struct AddEmployeeRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD(surname, std::string, "surname");
  REGISTER_STRUCT_FIELD(role, std::string, "role");
  REGISTER_STRUCT_FIELD_OPTIONAL(patronymic, std::string, "patronymic");
};

struct AddEmployeeResponse : public JsonCompatible {
  AddEmployeeResponse(const std::string& l, const std::string& p) {
    login = l;
    password = p;
  }

  REGISTER_STRUCT_FIELD(login, std::string, "login");
  REGISTER_STRUCT_FIELD(password, std::string, "password");
};

views::v1::reverse_index::ReverseIndexResponse AddReverseIndexFunc(
    const views::v1::reverse_index::ReverseIndexRequest& request) {
  std::vector<std::optional<std::string>> fields = {
      request.name,        request.surname, request.patronymic,
      request.role,        request.email,   request.birthday,
      request.telegram_id, request.vk_id,   request.team};

  if (request.phones.has_value()) {
    fields.insert(fields.end(), request.phones.value().begin(),
                  request.phones.value().end());
  }

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(request.employee_id);

  for (const auto& field : fields) {
    if (field.has_value()) {
      auto separator = (parameters.Size() == 1 ? "[" : ", ");
      parameters.PushBack(field.value());
      filter += fmt::format("{}${}", separator, parameters.Size());
    }
  }

  auto result = request.cluster->Execute(
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

  views::v1::reverse_index::ReverseIndexResponse response(request.employee_id);

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

    views::v1::reverse_index::ReverseIndexRequest r_index_request{
        [](const views::v1::reverse_index::ReverseIndexRequest& r)
            -> views::v1::reverse_index::ReverseIndexResponse {
          return AddReverseIndexFunc(std::move(r));
        },
        pg_cluster_,
        id,
        request_body.name,
        request_body.surname,
        request_body.patronymic,
        request_body.role};

    views::v1::reverse_index::ReverseIndexHandler(r_index_request);

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