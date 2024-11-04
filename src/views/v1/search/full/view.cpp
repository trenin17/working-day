#define V1_SEARCH_FULL

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>
#include <vector>

#include "core/json_compatible/struct.hpp"
#include "core/reverse_index/view.hpp"
#include "definitions/all.hpp"

#include "utils/s3_presigned_links.hpp"

namespace views::v1::search_full {

namespace {

std::vector<std::string> GetIds(const auto& id_sets, const int& limit) {
  std::unordered_map<std::string, double> m;
  for (const auto& el : id_sets) {
    for (const std::string& id : el.ids) {
      m[id] += el.similarity_score;
    }
  }

  std::vector<std::pair<std::string, double>> ids(m.begin(), m.end());
  std::sort(ids.begin(), ids.end(),
            [](auto& left, auto& right) { return left.second > right.second; });

  std::vector<std::string> final_ids;
  for (size_t i = 0; i < limit; ++i) {
    if (i < ids.size()) {
      final_ids.push_back(ids[i].first);
    }
  }

  return final_ids;
}

std::vector<std::string> SplitBySpaces(std::string str) {
  std::string s;
  std::stringstream ss(str);
  std::vector<std::string> v;
  while (std::getline(ss, s, ' ')) {
    v.push_back(core::reverse_index::ConvertToLower(s));
  }
  return v;
}

class SearchFullHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-search-full";

  SearchFullHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  struct IDsRow {
    std::unordered_set<std::string> ids;
    double similarity_score;
  };

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& company_id = ctx.GetData<std::string>("company_id");

    // lambda to add parameters
    auto append = [](const auto& value,
                     userver::storages::postgres::ParameterStore& parameters,
                     std::string& filter) {
      auto separator = (parameters.IsEmpty() ? "" : ", ");
      parameters.PushBack(value);
      filter += fmt::format("{}${}", separator, parameters.Size());
    };

    SearchFullRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    std::vector<std::string> search_keys =
        SplitBySpaces(request_body.search_key);

    // Getting a vector of sets of ids
    userver::storages::postgres::ParameterStore parameters;
    std::string filter;
    for (auto& key : search_keys) {
      append(key, parameters, filter);
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT ids, similarity(search_key, key) AS similarity_score "
        "FROM working_day_" +
            company_id +
            ".reverse_index, "
            "LATERAL unnest(ARRAY[" +
            filter +
            "]) AS search_key "
            "WHERE similarity(search_key, key) > 0.4; ",
        parameters);

    auto id_sets = result.AsContainer<std::vector<IDsRow>>(
        userver::storages::postgres::kRowTag);

    // Intersecting sets

    std::vector<std::string> final_ids = GetIds(id_sets, request_body.limit);

    // fetching ids' values and returning them

    SearchResponse response;

    userver::storages::postgres::ParameterStore parameters_fetch;
    std::string filter_fetch;

    int cnt = 0;
    for (auto& val : final_ids) {
      if (cnt >= request_body.limit) {
        break;
      }
      append(val, parameters_fetch, filter_fetch);
      cnt++;
    }

    if (parameters_fetch.Size() != 0) {
      auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT c.id, c.name, c.surname, c.patronymic, c.photo_link "
          "FROM working_day_" +
              company_id +
              ".employees AS c "
              "JOIN unnest(ARRAY[" +
              filter_fetch +
              "]) WITH ORDINALITY t(id, ord) USING (id) "
              "ORDER BY t.ord; ",
          parameters_fetch);

      response.employees = result.AsContainer<std::vector<ListEmployee>>(
          userver::storages::postgres::kRowTag);
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

void AppendSearchFull(userver::components::ComponentList& component_list) {
  component_list.Append<SearchFullHandler>();
}

}  // namespace views::v1::search_full