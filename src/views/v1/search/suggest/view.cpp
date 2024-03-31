#define V1_SEARCH_SUGGEST

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <sstream>
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
#include "definitions/all.hpp"

#include "utils/s3_presigned_links.hpp"

namespace views::v1::search_suggest {

namespace {

std::set<std::string> GetSuggestIds(auto& id_sets, auto& suggest_ids,
                                    size_t n_keys) {
  if (id_sets.size() != n_keys - 1 || suggest_ids.size() == 0) {
    return {};
  }

  std::set<std::string> uni;
  for (const auto& s : suggest_ids) {
    std::set<std::string> t;
    set_union(uni.begin(), uni.end(), s.ids.begin(), s.ids.end(),
              std::inserter(t, t.begin()));
    uni = t;
  }

  std::set<std::string> res = uni;

  for (const auto& set : id_sets) {
    std::set<std::string> t;
    set_intersection(res.begin(), res.end(), set.ids.begin(), set.ids.end(),
                     std::inserter(t, t.begin()));
    res = t;
  }

  std::set<std::string> final;
  const int suggest_size = 5;
  for (int i = 0; i < suggest_size; ++i) {
    if (!res.empty())
      final.insert(res.extract(res.begin()).value());
    else
      break;
  }

  return final;
}

std::vector<std::string> SplitBySpaces(std::string& str) {
  if (str.empty()) {
    return {""};
  }
  std::string s;
  std::stringstream ss(str);
  std::vector<std::string> v;
  while (std::getline(ss, s, ' ')) {
    // UNCOMMENT WHEN MERGED
    // transform(s.begin(), s.end(), s.begin(), ::tolower);
    v.push_back(s);
  }
  return v;
}

class SearchSuggestHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-search-suggest";

  SearchSuggestHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  struct IDsRow {
    std::set<std::string> ids;
  };

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    // lambda to add parameters
    auto append = [](const auto& value,
                     userver::storages::postgres::ParameterStore& parameters,
                     std::string& filter) {
      auto separator = (parameters.IsEmpty() ? "(" : ", ");
      parameters.PushBack(value);
      filter += fmt::format("{}${}", separator, parameters.Size());
    };

    SearchSuggestRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    std::vector<std::string> search_keys =
        SplitBySpaces(request_body.search_key);

    // Getting a vector of sets of ids
    userver::storages::postgres::ParameterStore parameters;
    std::string filter;
    for (size_t i = 0; i < search_keys.size() - 1; ++i) {
      append(search_keys[i], parameters, filter);
    }

    std::vector<IDsRow> id_sets;
    if (parameters.Size() > 0) {
      auto result1 = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT ids "
          "FROM working_day.reverse_index "
          "WHERE key IN " +
              filter + ");",
          parameters);

      id_sets = result1.AsContainer<std::vector<IDsRow>>(
          userver::storages::postgres::kRowTag);
    }

    auto result2 = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT ids "
        "FROM working_day.reverse_index "
        "WHERE key LIKE '" +
            search_keys[search_keys.size() - 1] +
            "%' "
            "ORDER BY key;");

    auto suggest_id_sets = result2.AsContainer<std::vector<IDsRow>>(
        userver::storages::postgres::kRowTag);

    // Intersecting sets

    std::set<std::string> final_ids =
        GetSuggestIds(id_sets, suggest_id_sets, search_keys.size());

    // fetching ids' values and returning them

    SearchResponse response;

    userver::storages::postgres::ParameterStore parameters_fetch;
    std::string filter_fetch;

    for (auto& val : final_ids) {
      append(val, parameters_fetch, filter_fetch);
    }

    if (parameters_fetch.Size() != 0) {
      auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT id, name, surname, patronymic, photo_link "
          "FROM working_day.employees "
          "WHERE id IN " +
              filter_fetch + ");",
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

void AppendSearchSuggest(userver::components::ComponentList& component_list) {
  component_list.Append<SearchSuggestHandler>();
}

}  // namespace views::v1::search_suggest