#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::employee::info {

namespace {

class HeadInfo {
 public:
  json ToJSONObject() const {
    json j;
    j["name"] = name;
    j["surname"] = surname;

    return j;
  }

  std::string name, surname;
};

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
    if (phones) {
      j["phones"] = phones.value();
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
    if (telegram_id) {
      j["telegram_id"] = telegram_id.value();
    }
    if (vk_id) {
      j["vk_id"] = vk_id.value();
    }
    if (team) {
      j["team"] = team.value();
    }
    if (head_info) {
      j["head_info"] = head_info.value().ToJSONObject();
    }

    return j.dump();
  }

  std::string id, name, surname;
  std::optional<std::string> patronymic, photo_link;
  std::optional<std::vector<std::string> > phones;
  std::optional<std::string> email, birthday, password, head_id, telegram_id,
      vk_id, team;
  std::optional<HeadInfo> head_info;
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
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");
    auto employee_id = request.GetArg("employee_id");

    if (employee_id.empty()) {
      employee_id = user_id;
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT employees.id, employees.name, employees.surname, "
        "employees.patronymic, "
        "employees.photo_link, employees.phones, employees.email, "
        "employees.birthday, employees.password, employees.head_id, "
        "employees.telegram_id, employees.vk_id, employees.team, "
        "case when employees.head_id is null then null else ROW (heads.name, "
        "heads.surname) end "
        "FROM working_day_" + company_id + ".employees as employees "
        "LEFT JOIN working_day_" + company_id + ".employees as heads "
        "ON employees.head_id = heads.id "
        "WHERE employees.id = $1",
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