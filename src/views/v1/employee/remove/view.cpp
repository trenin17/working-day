#include "view.hpp"
#include "core/reverse_index/view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

#include "core/json_compatible/struct.hpp"

namespace views::v1::employee::remove {

namespace {

struct ErrorMessage: public JsonCompatible {
  ErrorMessage(const std::string& msg) {
    message = msg;
  }

  REGISTER_STRUCT_FIELD(message, std::string, "message");
};

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
      userver::server::request::RequestContext&) const override {
    //CORS
    request.GetHttpResponse()
        .SetHeader(static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse()
        .SetHeader(static_cast<std::string>("Access-Control-Allow-Headers"), "*");
    
    const auto& employee_id = request.GetArg("employee_id");

    if (employee_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      ErrorMessage err_msg("Unauthorized");
      return err_msg.ToJsonString();
    }

    std::vector<std::optional<std::string>> old_values =
        views::v1::reverse_index::GetAllFields(pg_cluster_, employee_id);
    views::v1::reverse_index::ReverseIndexRequest r_index_request{
        pg_cluster_, employee_id, std::move(old_values)};

    views::v1::reverse_index::DeleteReverseIndex(r_index_request);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "DELETE FROM working_day.employees "
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