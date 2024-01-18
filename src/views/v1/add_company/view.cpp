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
#include "userver/utils/async.hpp"
#include <userver/engine/task/task_with_result.hpp>


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

    if (request_body.company_name.empty() || request_body.ceo_name.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      return ErrorMessage{"Names are empty"}.ToJSON();
    }

    auto tasks = tasks_.Lock();
    tasks->push_back(
      userver::utils::AsyncBackground("AsyncBackgroundTest", userver::engine::current_task::GetTaskProcessor(),
          [this, r = std::move(request_body)]() -> AddCompanyResponse {return this->AddFunc(r);}));

    return "";
  }

 private:
  AddCompanyResponse AddFunc(AddCompanyRequest request_body) const {
      auto id = userver::utils::generators::GenerateUuid();
      
      auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO working_day.companies(id, name, ceo_id) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (id) "
            "DO NOTHING;",
            id, request_body.company_name, request_body.ceo_name);

      AddCompanyResponse response(id);

      return response;
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  mutable userver::concurrent::Variable<std::vector<userver::engine::TaskWithResult<AddCompanyResponse>>> tasks_;
};

}  // namespace

void AppendAddCompany(userver::components::ComponentList& component_list) {
  component_list.Append<AddCompanyHandler>();
}

}  // namespace views::v1::add_company