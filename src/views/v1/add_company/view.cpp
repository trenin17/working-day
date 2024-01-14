#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/engine/task/task.hpp>


using json = nlohmann::json;

namespace views::v1::add_company {

namespace {

class AddCompanyRequest {
 public:
  AddCompanyRequest(const std::string& body) {
    auto j = json::parse(body);
    company_name = j["company_name"];
    ceo_name = j["ceo_name"];
  }

  std::string company_name, ceo_name;
};

class AddCompanyResponse {
 public:
  AddCompanyResponse(const std::string& id)
      : id(id) {}

  std::string ToJSON() const {
    nlohmann::json j;
    j["id"] = id;
    return j.dump();
  }

  std::string id;
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
/*
void AsyncFunc(userver::storages::postgres::ClusterPtr pg_cluster, std::string id, std::string company_name, std::string ceo_name) {
  
  LOG_INFO() << "Before";

  auto result = pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.companies(id, name, ceo_id) ",
        "VALUES ($1, $2, $3) "
        "ON CONFLICT (id) "
        "DO NOTHING;",
        id, company_name, ceo_name);

  LOG_INFO() << "After";
}*/

class AddCompanyHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-add-company";

  AddCompanyHandler(
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

    AddCompanyRequest request_body(request.RequestBody());

    auto id = userver::utils::generators::GenerateUuid();

    if (request_body.company_name.empty() || request_body.ceo_name.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      return ErrorMessage{"Names are empty"}.ToJSON();
    }


    LOG_INFO() << "Before";

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT id, name, ceo_id FROM working_day.companies WHERE name = $1",
        request_body.company_name);

    LOG_INFO() << result.AsSingleRow<std::string>();

    LOG_INFO() << "After";


    /*std::string SQLResponse = result.AsSingleRow<std::string>();
    if (SQLResponse.empty()) {
      std::cout << "empty" << std::endl;
    }
    std::cout << SQLResponse << std::endl;*/

    AddCompanyResponse response(id);

    return response.ToJSON();

  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAddCompany(userver::components::ComponentList& component_list) {
  component_list.Append<AddCompanyHandler>();
}

}  // namespace views::v1::add_company