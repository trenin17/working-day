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

struct IdsItem {
  std::string ids;
  std::string entity_type;
};

std::vector<IdsItem> GetIds(const auto& id_sets, const int& limit, const std::string& entity_type) {
  std::unordered_map<std::string, std::pair<double, std::string>> m;
  for (const auto& el : id_sets) {
    for (const std::string& id : el.ids) {
      m[id].first += el.similarity_score;
      m[id].second = el.entity_type;
    }
  }

  std::vector<std::pair<std::string, double>> ids;
  for (const auto& [id, data] : m) {
    ids.emplace_back(id, data.first);
  }
  std::sort(ids.begin(), ids.end(),
            [](auto& left, auto& right) { return left.second > right.second; });

  // (id of entity, entity_type)
  std::vector<IdsItem> final_ids;
  for (size_t i = 0; i < limit && i < ids.size(); ++i) {
    IdsItem cur_item = {ids[i].first, m[ids[i].first].second};
    final_ids.emplace_back(cur_item);
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
    std::string entity_type;
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

    std::string tag = request_body.tag.value_or("employees");
    std::string entity_filter = (tag == "all") 
                                ? "" 
                                : "AND entity_type = $" +  std::to_string(parameters.Size() + 1);
    
    if (tag != "all") {
      parameters.PushBack(tag);
    }

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT ids, entity_type, similarity(search_key, key) AS similarity_score "
        "FROM working_day_" +
            company_id +
            ".reverse_index, "
            "LATERAL unnest(ARRAY[" +
            filter +
            "]) AS search_key "
            "WHERE similarity(search_key, key) > 0.4 " + 
            entity_filter + ";",
        parameters);

    auto id_sets = result.AsContainer<std::vector<IDsRow>>(
        userver::storages::postgres::kRowTag);

    // Intersecting sets

    std::vector<IdsItem> final_ids = GetIds(id_sets, request_body.limit, tag);

    // fetching ids' values and returning them

    SearchResponse response;

    userver::storages::postgres::ParameterStore parameters_employee, parameters_task;
    std::string filter_employee, filter_task;

    int cnt = 0;

    for (auto& val : final_ids) {
      if (cnt >= request_body.limit) {
        break;
      }
      if (val.entity_type == "employees") {
        append(val.ids, parameters_employee, filter_employee);
      } else if (val.entity_type == "tasks") {
        append(val.ids, parameters_task, filter_task);
      } 
      cnt++;
    }

    if (parameters_employee.Size() != 0) {
      auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT c.id, c.name, c.surname, c.patronymic, c.photo_link "
          "FROM working_day_" +
              company_id +
              ".employees AS c "
              "JOIN unnest(ARRAY[" +
              filter_employee +
              "]) WITH ORDINALITY t(id, ord) USING (id) "
              "ORDER BY t.ord; ",
          parameters_employee);

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

    if (parameters_task.Size() != 0) {
      auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT t.title, t.project_name, t.id, t.creator, t.assignee "
          "FROM working_day_" +
              company_id +
              ".tracker_tasks AS t "
              "JOIN unnest(ARRAY[" +
              filter_task +
              "]) WITH ORDINALITY u(id, ord) USING (id) "
              "ORDER BY u.ord; ",
              parameters_task);

      response.tasks = result.AsContainer<std::vector<TrackerTasksListItem>>(
          userver::storages::postgres::kRowTag);
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