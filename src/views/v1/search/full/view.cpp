#define V1_SEARCH_FULL

#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>
#include <set>
#include <vector>
#include <algorithm>
#include <sstream>

#include "core/json_compatible/struct.hpp"
#include "definitions/all.hpp"

#include "utils/s3_presigned_links.hpp"

namespace views::v1::search_full {

namespace {

bool NextPerm(std::vector<bool>& p) {
    if (p.size() == 0) {
        return false;
    }
    int i = p.size() - 1;
    bool flag = false;
    int ones = 0;
    int zeros = 0;
    while (i >= 1) {
        if (p[i]) {
            ones++;
        } else {
            zeros++;
        }
        if (!p[i] && p[i - 1]){
            flag = true;
            break;
        }
        --i;
    }
    if (!flag && p[0]) {
        return false;
    }
    if (flag) {
        int end = p.size() - 1;
        for (int i = 0; i <= zeros + ones; ++i) {
            if (i < zeros - 1) {
                p[end - i] = 0;
            } else if (i < zeros + ones) {
                p[end - i] = 1;
            } else if (i == zeros + ones) {
                p[end - i] = 0;
            }
        }
    } else {
        for (int i = 0; i < p.size(); ++i) {
            if (i < ones + 1) {
                p[i] = 1;
            } else {
                p[i] = 0;
            }
        }
    }
    return true;
}

std::set<std::string> GetIds(const auto& id_sets, const int& limit) {
    std::set<std::string> final_ids;
    std::vector<bool> perm(id_sets.size());

    do {
        std::set<std::string> res;
        bool flag = false;
        for (size_t i = 0; i < perm.size(); ++i) {
            if (perm[i]) {
                continue;
            }
            if (!flag) {
                res = id_sets[i].ids;
                flag = true;
            } else {
                std::set<std::string> t;
                set_intersection(res.begin(), res.end(), 
                                 id_sets[i].ids.begin(), id_sets[i].ids.end(),
                                 std::inserter(t, t.begin()));
                res = t;
            }
        }

        final_ids.insert(res.begin(), res.end());

    } while (NextPerm(perm) && final_ids.size() < limit);

    return final_ids;
}

std::vector<std::string> SplitBySpaces(std::string& str) {
    std::string s;
    std::stringstream ss(str);
    std::vector<std::string> v;
    while (std::getline(ss, s, ' ')) {
        v.push_back(s);
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
    auto append = [](const auto& value, userver::storages::postgres::ParameterStore& parameters, std::string& filter) {
        auto separator = (parameters.IsEmpty() ? "(" : ", ");
        parameters.PushBack(value);
        filter += fmt::format("{}${}", separator, parameters.Size());        
    };    

    SearchFullRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    std::vector<std::string> search_keys = SplitBySpaces(request_body.search_key);

    // Getting a vector of sets of ids
    userver::storages::postgres::ParameterStore parameters;
    std::string filter;
    for (auto& key : search_keys) {
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        append(key, parameters, filter);
    }

    auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "SELECT ids "
            "FROM working_day.reverse_index "
            "WHERE key IN " + filter + ");",
            parameters);
    
    auto id_sets = result.AsContainer<std::vector<IDsRow>>(
        userver::storages::postgres::kRowTag);

    // Intersecting sets

    std::set<std::string> final_ids = GetIds(id_sets, request_body.limit);

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
            "WHERE id IN " + filter_fetch + ");",
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