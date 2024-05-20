#define V1_ADD_EMPLOYEE

#include "view.hpp"

#include <codecvt>
#include <locale>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "definitions/all.hpp"

#include "core/reverse_index/view.hpp"

namespace views::v1::employee::add {

namespace {

core::reverse_index::ReverseIndexResponse AddReverseIndexFunc(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::EmployeeAllData data) {
  std::vector<std::optional<std::string>> fields = {
      data.name,     data.surname,     data.patronymic, data.role, data.email,
      data.birthday, data.telegram_id, data.vk_id,      data.team};

  if (data.phones.has_value()) {
    fields.insert(fields.end(), data.phones.value().begin(),
                  data.phones.value().end());
  }

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(data.employee_id);

  for (auto& field : fields) {
    if (field.has_value()) {
      auto separator = (parameters.Size() == 1 ? "[" : ", ");
      field.value() = core::reverse_index::ConvertToLower(field.value());
      parameters.PushBack(field.value());
      filter += fmt::format("{}${}", separator, parameters.Size());
    }
  }

  auto result = cluster->Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      "WITH input_data AS ( "
      "  SELECT ARRAY" +
          filter +
          "] AS keys, $1 AS id "
          ") "
          "INSERT INTO working_day.reverse_index (key, ids) "
          "SELECT key, ARRAY[id] AS ids "
          "FROM input_data, LATERAL unnest(keys) AS key "
          "ON CONFLICT (key) DO UPDATE "
          "SET ids = array_append(working_day.reverse_index.ids, "
          "EXCLUDED.ids[1]); ",
      parameters);

  core::reverse_index::ReverseIndexResponse response(data.employee_id);

  return response;
}

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

std::string generateLogin(
    const std::string& firstName, const std::string& lastName,
    const userver::storages::postgres::ClusterPtr& cluster) {
  std::string transliteratedFirst = Transliterate(firstName);
  std::string transliteratedLast = Transliterate(lastName);

  std::string baseLogin = transliteratedFirst.substr(0, 1) + transliteratedLast;
  std::string newLogin = baseLogin;
  int counter = 0;

  while (true) {
    auto result = cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT id FROM working_day.employees WHERE id = $1", newLogin);
    if (result.Size() == 0) {
      break;
    }
    newLogin = baseLogin + std::to_string(++counter);
  }

  return newLogin;
}

class AddEmployeeHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-employee-add";

  AddEmployeeHandler(
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

    AddEmployeeRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());
    auto company_id = ctx.GetData<std::string>("company_id");

    auto id =
        generateLogin(request_body.name, request_body.surname, pg_cluster_);
    auto password = userver::utils::generators::GenerateUuid().substr(0, 16);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.employees(id, name, surname, patronymic, "
        "password, role, company_id) VALUES($1, $2, $3, $4, $5, $6, $7) "
        "ON CONFLICT (id) "
        "DO NOTHING",
        id, request_body.name, request_body.surname, request_body.patronymic,
        password, request_body.role, company_id);

    core::reverse_index::EmployeeAllData data{
        id, request_body.name, request_body.surname, request_body.patronymic,
        request_body.role};

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data]() -> core::reverse_index::ReverseIndexResponse {
          return AddReverseIndexFunc(cluster, data);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    AddEmployeeResponse response(id, password);
    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendAddEmployee(userver::components::ComponentList& component_list) {
  component_list.Append<AddEmployeeHandler>();
}

}  // namespace views::v1::employee::add