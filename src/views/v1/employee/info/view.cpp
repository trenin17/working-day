#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::employee::info {

namespace {

class InfoEmployeeResponse {
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
    if (phone) {
      j["phone"] = phone.value();
    }
    if (email) {
      j["email"] = email.value();
    }
    if (birthday) {
      j["birthday"] = birthday.value();
    }
    if (password) {
      j["password"] = password.value();
    }
    if (head_id) {
      j["head_id"] = head_id.value();
    }

    return j.dump();
  }

  std::string id, name, surname;
  std::optional<std::string> patronymic, photo_link, phone, email, birthday,
      password, head_id;
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

class InfoEmployeeHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-info";

  InfoEmployeeHandler(
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
    const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& employee_id = request.GetArg("employee_id");

    if (employee_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      return ErrorMessage{"Unauthorized"}.ToJSON();
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, name, surname, patronymic, photo_link, phone, email, "
        "birthday, password, head_id "
        "FROM working_day.employees "
        "WHERE id = $1",
        employee_id);

    if (result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Not Found"}.ToJSON();
    }

    InfoEmployeeResponse response{result.AsSingleRow<InfoEmployeeResponse>(
        userver::storages::postgres::kRowTag)};

    if (response.photo_link.has_value()) {
      response.photo_link =
          utils::s3_presigned_links::GeneratePhotoPresignedLink(
              response.photo_link.value(), utils::s3_presigned_links::Download);
    }

    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendInfoEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<InfoEmployeeHandler>();
}

}  // namespace views::v1::employee::info