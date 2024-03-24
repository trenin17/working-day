#define V1_EDIT_EMPLOYEE

#include "view.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/http/http.h>
#include <aws/s3/S3Client.h>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "core/json_compatible/struct.hpp"
#include "core/reverse_index/view.hpp"

#include "definitions/all.hpp"

namespace views::v1::profile::edit {

namespace {

struct EditValuesRow {
  std::optional<std::vector<std::string>> phones;
  std::optional<std::string> email, birthday, telegram_id, vk_id, team;
};

core::reverse_index::EmployeeAllData FetchOldData(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::EmployeeAllData data) {
  auto grab_result =
      cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       "SELECT CASE WHEN $2 IS NULL THEN NULL ELSE phones END, "
                       "CASE WHEN $3 IS NULL THEN NULL ELSE email END, "
                       "CASE WHEN $4 IS NULL THEN NULL ELSE birthday END, "
                       "CASE WHEN $5 IS NULL THEN NULL ELSE telegram_id END, "
                       "CASE WHEN $6 IS NULL THEN NULL ELSE vk_id END, "
                       "CASE WHEN $7 IS NULL THEN NULL ELSE team END "
                       "FROM working_day.employees "
                       "WHERE id = $1; ",
                       data.employee_id, data.phones, data.email, data.birthday,
                       data.telegram_id, data.vk_id, data.team);

  auto old_values = grab_result.AsSingleRow<EditValuesRow>(
      userver::storages::postgres::kRowTag);

  core::reverse_index::EmployeeAllData data_old{
      data.employee_id,    std::nullopt,           std::nullopt,
      std::nullopt,        std::nullopt,           old_values.email,
      old_values.birthday, old_values.telegram_id, old_values.vk_id,
      old_values.team,     old_values.phones};

  return data_old;
}

core::reverse_index::ReverseIndexResponse EditReverseIndexFunc(
    userver::storages::postgres::ClusterPtr cluster,
    core::reverse_index::EmployeeAllData old_data,
    core::reverse_index::EmployeeAllData new_data) {
  std::vector<std::optional<std::string>> old_values_vec = {
      old_data.email, old_data.birthday, old_data.telegram_id, old_data.vk_id,
      old_data.team};

  if (old_data.phones.has_value()) {
    old_values_vec.insert(old_values_vec.end(), old_data.phones.value().begin(),
                          old_data.phones.value().end());
  }

  userver::storages::postgres::ParameterStore parameters;
  std::string filter;

  parameters.PushBack(old_data.employee_id);

  for (const auto& field : old_values_vec) {
    if (field.has_value()) {
      auto separator = (parameters.Size() == 1 ? "(" : ", ");
      parameters.PushBack(field.value());
      filter += fmt::format("{}${}", separator, parameters.Size());
    }
  }

  if (parameters.Size() > 1) {
    auto result =
        cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "UPDATE working_day.reverse_index "
                         "SET ids = array_remove(ids, $1) "
                         "WHERE key IN " +
                             filter + "); ",
                         parameters);
  }

  std::vector<std::optional<std::string>> new_fields = {
      new_data.name,        new_data.surname, new_data.patronymic,
      new_data.role,        new_data.email,   new_data.birthday,
      new_data.telegram_id, new_data.vk_id,   new_data.team};

  if (new_data.phones.has_value()) {
    new_fields.insert(new_fields.end(), new_data.phones.value().begin(),
                      new_data.phones.value().end());
  }

  userver::storages::postgres::ParameterStore parameters2;
  std::string filter2;

  parameters2.PushBack(new_data.employee_id);

  for (auto& field : new_fields) {
    if (field.has_value()) {
      auto separator = (parameters2.Size() == 1 ? "[" : ", ");
      transform(field.value().begin(), field.value().end(),
                field.value().begin(), ::tolower);
      parameters2.PushBack(field.value());
      filter2 += fmt::format("{}${}", separator, parameters2.Size());
    }
  }

  if (parameters2.Size() > 1) {
    auto result2 = cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
            "WITH input_data AS ( "
            "  SELECT ARRAY" +
            filter2 +
            "] AS keys, $1 AS id "
            ") "
            "INSERT INTO working_day.reverse_index (key, ids) "
            "SELECT key, ARRAY[id] AS ids "
            "FROM input_data, LATERAL unnest(keys) AS key "
            "ON CONFLICT (key) DO UPDATE "
            "SET ids = array_append(working_day.reverse_index.ids, "
            "EXCLUDED.ids[1]); ",
        parameters2);
  }

  core::reverse_index::ReverseIndexResponse response(new_data.employee_id);
  return response;
}

class ProfileEditHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-profile-edit";

  ProfileEditHandler(
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

    ProfileEditRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    core::reverse_index::EmployeeAllData data_new{user_id,
                                                  std::nullopt,
                                                  std::nullopt,
                                                  std::nullopt,
                                                  std::nullopt,
                                                  request_body.email,
                                                  request_body.birthday,
                                                  request_body.telegram_id,
                                                  request_body.vk_id,
                                                  request_body.team,
                                                  request_body.phones};

    core::reverse_index::EmployeeAllData data_old =
        FetchOldData(pg_cluster_, data_new);

    userver::storages::postgres::ClusterPtr cluster = pg_cluster_;
    core::reverse_index::ReverseIndexRequest r_index_request{
        [cluster, data_old,
         data_new]() -> core::reverse_index::ReverseIndexResponse {
          return EditReverseIndexFunc(cluster, data_old, data_new);
        }};

    core::reverse_index::ReverseIndexHandler(r_index_request);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day.employees "
        "SET phones = case when $2 is null then phones else $2 end, "
        "email = case when $3 is null then email else $3 end, "
        "birthday = case when $4 is null then birthday else $4 end, "
        "password = case when $5 is null then password else $5 end, "
        "telegram_id = case when $6 is null then telegram_id else $6 end, "
        "vk_id = case when $7 is null then vk_id else $7 end, "
        "team = case when $8 is null then team else $8 end "
        "WHERE id = $1",
        user_id, request_body.phones, request_body.email, request_body.birthday,
        request_body.password, request_body.telegram_id, request_body.vk_id,
        request_body.team);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendProfileEdit(userver::components::ComponentList& component_list) {
  component_list.Append<ProfileEditHandler>();
}

}  // namespace views::v1::profile::edit