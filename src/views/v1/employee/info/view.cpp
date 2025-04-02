#define V1_EMPLOYEE_INFO

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

#include "definitions/all.hpp"

namespace views::v1::employee::info {

namespace {

struct debugPosition {
  std::optional<std::string> job_position;
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

    auto debug = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT job_position FROM working_day_" + company_id +
            ".employees WHERE id = $1",
        employee_id);

    auto debug_result = debug.AsSingleRow<debugPosition>(
        userver::storages::postgres::kRowTag);
        
    LOG_INFO() << "DEBUG JOB POSITION HAS VALUE " << debug_result.job_position.has_value();
    if (debug_result.job_position.has_value()) {
      LOG_INFO() << "DEBUG JOB POSITION " << debug_result.job_position.value();
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT employees.id, employees.name, employees.surname, "
        "employees.patronymic, "
        "employees.photo_link, employees.phones, employees.email, "
        "employees.birthday, employees.password, employees.head_id, "
        "employees.telegram_id, employees.vk_id, employees.team, "
        "case when employees.head_id is null then null else ROW (heads.id, "
        "heads.name, "
        "heads.surname, NULL::TEXT, NULL::TEXT) end as head_info, employees.inventory, employees.job_position "
        "FROM working_day_" +
            company_id +
            ".employees as employees "
            "LEFT JOIN working_day_" +
            company_id +
            ".employees as heads "
            "ON employees.head_id = heads.id "
            "WHERE employees.id = $1",
        employee_id);

    if (result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Not Found"}.ToJsonString();
    }

    Employee response{
        result.AsSingleRow<Employee>(userver::storages::postgres::kRowTag)};
    
    LOG_INFO() << "JOB POSITION HAS VALUE " << response.job_position.has_value();
    if (response.job_position.has_value()) {
      LOG_INFO() << "JOB POSITION " << response.job_position.value();
    }

    if (response.photo_link.has_value()) {
      response.photo_link =
          utils::s3_presigned_links::GeneratePhotoPresignedLink(
              response.photo_link.value(), utils::s3_presigned_links::Download);
    }

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendInfoEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<InfoEmployeeHandler>();
}

}  // namespace views::v1::employee::info