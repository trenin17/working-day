#include "view.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/http/http.h>
#include <aws/s3/S3Client.h>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

#include "core/json_compatible/struct.hpp"
#include "core/reverse_index/view.hpp"

namespace views::v1::profile::edit {

namespace {

struct ProfileEditRequest: public JsonCompatible {
  REGISTER_STRUCT_FIELD_OPTIONAL(phones, std::vector<std::string>, "phones");
  REGISTER_STRUCT_FIELD_OPTIONAL(email, std::string, "email");
  REGISTER_STRUCT_FIELD_OPTIONAL(birthday, std::string, "birthday");
  REGISTER_STRUCT_FIELD_OPTIONAL(password, std::string, "password");
  REGISTER_STRUCT_FIELD_OPTIONAL(telegram_id, std::string, "telegram_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(vk_id, std::string, "vk_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(team, std::string, "team");
};

std::vector<std::string> GetEditTypes(ProfileEditRequest& request) {
  std::vector<std::string> types;
  if (request.phones.has_value()) {
    types.push_back("phones");
  }
  if (request.email.has_value()) {
    types.push_back("email");
  }
  if (request.birthday.has_value()) {
    types.push_back("birthday");
  }
  if (request.telegram_id.has_value()) {
    types.push_back("telegram_id");
  }
  if (request.vk_id.has_value()) {
    types.push_back("vk_id");
  }
  if (request.team.has_value()) {
    types.push_back("team");
  }
  return types;
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
    //CORS
    request.GetHttpResponse()
        .SetHeader(static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse()
        .SetHeader(static_cast<std::string>("Access-Control-Allow-Headers"), "*");
    
    const auto& user_id = ctx.GetData<std::string>("user_id");

    ProfileEditRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto edit_types = GetEditTypes(request_body);
    std::vector<std::optional<std::string>> old_values = views::v1::reverse_index::GetEditFields(pg_cluster_, user_id, edit_types);
    std::vector<std::optional<std::string>> new_values{
            request_body.email, request_body.birthday,
            request_body.telegram_id, request_body.vk_id, request_body.team};
    if (request_body.phones.has_value()) {
      new_values.insert(new_values.end(), request_body.phones.value().begin(), request_body.phones.value().end());
    }

    views::v1::reverse_index::EditIndexRequest r_index_request{
        pg_cluster_, user_id, 
        std::move(old_values), std::move(new_values)};

    views::v1::reverse_index::EditReverseIndex(r_index_request);

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
        request_body.password, request_body.telegram_id, request_body.vk_id, request_body.team);

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