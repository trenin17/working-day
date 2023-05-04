#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::employees {

namespace {

class ListEmployee {
 public:
  std::string ToJSON() const {
    json j;
    j["id"] = id;
    j["name"] = name;
    j["surname"] = surname;
    if (patronymic) {
      j["patronymic"] = patronymic.value();
    }
    if (photo_link) {
      j["photo_link"] = photo_link.value();
    }

    return j.dump();
  }

  std::string id, name, surname;
  std::optional<std::string> patronymic, photo_link;
};

class EmployeesResponse {
 public:
  std::string ToJSON() const {
    json j = json::array();
    for (const auto& employee : employees) {
      j.push_back(employee.ToJSON());
    }
    return j.dump();
  }

  std::vector<ListEmployee> employees;
};

class EmployeesHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employees";

  EmployeesHandler(
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
    const auto& user_id = request.GetHeader("user_id");

    if (user_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      return "Unauthorized";
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, name, surname, patronymic, photo_link "
        "FROM working_day.employees "
        "WHERE head_id = $1",
        user_id);

    EmployeesResponse response{result.AsContainer<std::vector<ListEmployee>>(
        userver::storages::postgres::kRowTag)};
    for (auto& employee : response.employees) {
      if (employee.photo_link.has_value()) {
        employee.photo_link = utils::s3_presigned_links::GeneratePhotoPresignedLink(employee.photo_link.value(), utils::s3_presigned_links::Download);
      }
    }
    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendEmployees(userver::components::ComponentList& component_list) {
  component_list.Append<EmployeesHandler>();
}

}  // namespace views::v1::employees