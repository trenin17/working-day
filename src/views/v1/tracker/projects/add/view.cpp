#define V1_TRACKER_PROJECTS_ADD
#define USERVER_POSTGRES_ENABLE_LEGACY_TIMESTAMP 1

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/utils/text.hpp>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <unordered_map>


#include "definitions/all.hpp"

#include "core/reverse_index/view.hpp"


namespace views::v1::tracker::projects::add {

namespace {

std::string Char32ToString(char32_t ch) {
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
  std::string result = convert.to_bytes(ch);
  return result;
}

std::string Transliterate(const std::string& cyrillic) {
  const std::unordered_map<std::string, std::string> translit_table = {
      {"А", "a"},  {"Б", "b"},   {"В", "v"},  {"Г", "g"},  {"Д", "d"},
      {"Е", "e"},  {"Ё", "e"},   {"Ж", "zh"}, {"З", "z"},  {"И", "i"},
      {"Й", "i"},  {"К", "k"},   {"Л", "l"},  {"М", "m"},  {"Н", "n"},
      {"О", "o"},  {"П", "p"},   {"Р", "r"},  {"С", "s"},  {"Т", "t"},
      {"У", "u"},  {"Ф", "f"},   {"Х", "kh"}, {"Ц", "ts"}, {"Ч", "ch"},
      {"Ш", "sh"}, {"Щ", "sch"}, {"Ъ", ""},   {"Ы", "y"},  {"Ь", ""},
      {"Э", "e"},  {"Ю", "yu"},  {"Я", "ya"}, {"а", "a"},  {"б", "b"},
      {"в", "v"},  {"г", "g"},   {"д", "d"},  {"е", "e"},  {"ё", "e"},
      {"ж", "zh"}, {"з", "z"},   {"и", "i"},  {"й", "i"},  {"к", "k"},
      {"л", "l"},  {"м", "m"},   {"н", "n"},  {"о", "o"},  {"п", "p"},
      {"р", "r"},  {"с", "s"},   {"т", "t"},  {"у", "u"},  {"ф", "f"},
      {"х", "kh"}, {"ц", "ts"},  {"ч", "ch"}, {"ш", "sh"}, {"щ", "sch"},
      {"ъ", ""},   {"ы", "y"},   {"ь", ""},   {"э", "e"},  {"ю", "yu"},
      {"я", "ya"}};
  std::string result;
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
  auto u32string = convert.from_bytes(cyrillic);

  for (char32_t ch : u32string) {
    auto ch_str = Char32ToString(ch);
    if (auto it = translit_table.find(ch_str); it != translit_table.end()) {
      result += it->second;
    } else {
      result += std::tolower(ch);  // For latin symbols
    }
  }
  return result;
}


core::reverse_index::ReverseIndexResponse AddProjectToReverseIndexFunc(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::TrackerProjectsAllData data) {

    std::vector<std::string> words;
    std::istringstream stream(data.title.value());
    std::string word;

    while (stream >> word) {
        words.push_back(core::reverse_index::ConvertToLower(word));
    }

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(data.project_id);

  for (auto& w : words) {
    auto separator = (parameters.Size() == 1 ? "[" : ", ");
    parameters.PushBack(w);
    filter += fmt::format("{}${}", separator, parameters.Size());
  }

  auto result =
      cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "WITH input_data AS ( "
                       "  SELECT ARRAY" +
                           filter +
                           "] AS keys, $1 AS id "
                           ") "
                           "INSERT INTO working_day_" +
                           data.company_id +
                           ".reverse_index (key, ids, entity_type) "
                           "SELECT key, ARRAY[id] AS ids, 'projects' AS entity_type "
                           "FROM input_data, LATERAL unnest(keys) AS key "
                           "ON CONFLICT (key, entity_type) DO UPDATE "
                           "SET ids = array_append(working_day_" +
                           data.company_id +
                           ".reverse_index.ids, "
                           "EXCLUDED.ids[1]); ",
                       parameters);

  core::reverse_index::ReverseIndexResponse response(data.project_id);

  return response;
}


class TrackerProjectsAddHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-tracker-projects-add";

  TrackerProjectsAddHandler(
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

    TrackerProjectsItemRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto trx = pg_cluster_->Begin(
        "tracker_projects_add",
        userver::storages::postgres::ClusterHostType::kMaster, {});


    if (request_body.project_key.empty()) {
      return ErrorMessage{"Project key is required"}.ToJsonString();
    }
    std::string project_id = Transliterate(request_body.project_key);
    project_id = userver::utils::text::ToUpper(project_id);
    std::replace(project_id.begin(), project_id.end(), ' ', '_');


    if (request_body.assigned_users_ids) {
      auto check = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT id FROM working_day_" + company_id +
          ".employees WHERE id = ANY($1)",
          *request_body.assigned_users_ids);

      if (check.Size() != request_body.assigned_users_ids->size()) {
        request.GetHttpResponse().SetStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return ErrorMessage{
            "Invalid assigned_users_ids: one or more employees not found"}
            .ToJsonString();
      }
    }

    auto result = trx.Execute(
        "INSERT INTO working_day_" + company_id +
            ".tracker_projects (project_id, title, description, creator, status) "
            "VALUES ($1, $2, $3, $4, $5)",
        project_id,
        request_body.title,
        request_body.description,
        user_id,
        request_body.status.value_or("Open")
      );

    if (request_body.assigned_users_ids) {
      for (const auto& employee_id : *request_body.assigned_users_ids) {
        trx.Execute(
          "INSERT INTO working_day_" + company_id +
              ".tracker_project_assigned_users (project_id, employee_id) "
              "VALUES ($1, $2)  ON CONFLICT DO NOTHING",
          project_id,
          employee_id
        );
      }
    }
    trx.Commit();

    core::reverse_index::TrackerProjectsAllData data{project_id, request_body.title};
    data.company_id = company_id;

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data]() -> core::reverse_index::ReverseIndexResponse {
          return AddProjectToReverseIndexFunc(cluster, data);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    TrackerProjectsAddResponse response;
    response.project_id = project_id;
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendTrackerProjectsAdd(userver::components::ComponentList& component_list) {
  component_list.Append<TrackerProjectsAddHandler>();
}

}  // namespace views::v1::tracker::projects::add