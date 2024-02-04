#include "view.hpp"
#include "core/reverse_index/view.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/http/http.h>
#include <aws/s3/S3Client.h>
#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

using json = nlohmann::json;

namespace views::v1::profile::edit {

namespace {

class ProfileEditRequest {
 public:
  ProfileEditRequest(const std::string& body) {
    auto j = json::parse(body);

    if (j.contains("phones")) {
      phones = j["phones"];
      field_types.push_back("phones");
    }
    if (j.contains("email")) {
      email = j["email"];
      field_types.push_back("email");
    }
    if (j.contains("birthday")) {
      birthday = j["birthday"];
      field_types.push_back("birthday");
    }
    if (j.contains("password")) {
      password = j["password"];
    }
    if (j.contains("telegram_id")) {
      telegram_id = j["telegram_id"];
      field_types.push_back("telegram_id");
    }
    if (j.contains("vk_id")) {
      vk_id = j["vk_id"];
      field_types.push_back("vk_id");
    }
    if (j.contains("team")) {
      team = j["team"];
      field_types.push_back("team");
    }
  }

  std::optional<std::vector<std::string> > phones;
  std::optional<std::string> email, birthday, password,
      telegram_id, vk_id, team;
  std::vector<std::string> field_types;
};

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

    ProfileEditRequest request_body(request.RequestBody());

    std::vector<std::optional<std::string>> old_values = views::v1::reverse_index::GetEditFields(pg_cluster_, user_id, request_body.field_types);
    std::vector<std::optional<std::string>> new_values{
            request_body.email, request_body.birthday,
            request_body.password, request_body.telegram_id, request_body.vk_id, request_body.team};
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