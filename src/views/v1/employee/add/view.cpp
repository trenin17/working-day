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

namespace views::v1::employee::add {

namespace {

class AddEmployeeRequest {
 public:
  AddEmployeeRequest(const std::string& body) {
    auto j = json::parse(body);
    name = j["name"];
    surname = j["surname"];
    if (j.contains("patronymic")) {
      patronymic = j["patronymic"];
    }
    role = j["role"];
  }

  std::string name, surname, role;
  std::optional<std::string> patronymic;
};

class AddEmployeeResponse {
 public:
  AddEmployeeResponse(const std::string& l, const std::string& p)
      : login(l), password(p) {}

  std::string ToJSON() const {
    nlohmann::json j;
    j["login"] = login;
    j["password"] = password;
    return j.dump();
  }

  std::string login, password;
};

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
      userver::server::request::RequestContext&) const override {
    AddEmployeeRequest request_body(request.RequestBody());

    auto id = userver::utils::generators::GenerateUuid();
    auto password = userver::utils::generators::GenerateUuid();

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.employees(id, name, surname, patronymic, "
        "password, role) VALUES($1, $2, $3, $4, $5, $6) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        id, request_body.name, request_body.surname, request_body.patronymic,
        password, request_body.role);

    AddEmployeeResponse response(id, password);
    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAddEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<AddEmployeeHandler>();
}

}  // namespace views::v1::employee::add