#define V1_SEARCH_BASIC

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>

#include "definitions/all.hpp"
#include "core/reverse_index/view.hpp"
#include "utils/s3_presigned_links.hpp"

namespace views::v1::search_basic {

namespace {

class SearchBasicHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-search-basic";

  SearchBasicHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  struct IDsRow {
    std::vector<std::string> ids;
  };

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    SearchBasicRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    request_body.search_key = core::reverse_index::ConvertToLower(request_body.search_key);

    auto result_ids = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT ids "
        "FROM working_day.reverse_index "
        "WHERE key = $1;",
        request_body.search_key);

    auto IDs = result_ids.AsOptionalSingleRow<IDsRow>(
        userver::storages::postgres::kRowTag);

    SearchResponse response;

    if (IDs.has_value()) {
      userver::storages::postgres::ParameterStore parameters;
      std::string filter;

      auto append = [&](const auto& value) {
        auto separator = (parameters.IsEmpty() ? "(" : ", ");
        parameters.PushBack(value);
        filter += fmt::format("{}${}", separator, parameters.Size());
      };

      for (auto& val : IDs.value().ids) {
        append(val);
      }

      if (parameters.Size() != 0) {
        auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT id, name, surname, patronymic, photo_link "
            "FROM working_day.employees "
            "WHERE id IN " +
                filter + ");",
            parameters);

        response.employees = result.AsContainer<std::vector<ListEmployee>>(
            userver::storages::postgres::kRowTag);
      }
    }

    for (auto& employee : response.employees) {
      if (employee.photo_link.has_value()) {
        employee.photo_link =
            utils::s3_presigned_links::GeneratePhotoPresignedLink(
                employee.photo_link.value(),
                utils::s3_presigned_links::Download);
      }
    }

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendSearchBasic(userver::components::ComponentList& component_list) {
  component_list.Append<SearchBasicHandler>();
}

}  // namespace views::v1::search_basic