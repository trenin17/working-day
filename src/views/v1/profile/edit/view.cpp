#include "view.hpp"

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
    }
    if (j.contains("email")) {
      email = j["email"];
    }
    if (j.contains("birthday")) {
      birthday = j["birthday"];
    }
    if (j.contains("password")) {
      password = j["password"];
    }
    if (j.contains("telegram_id")) {
      telegram_id = j["telegram_id"];
    }
    if (j.contains("vk_id")) {
      vk_id = j["vk_id"];
    }
    if (j.contains("team")) {
      team = j["team"];
    }
  }

  std::optional<std::vector<std::string> > phones;
  std::optional<std::string> email, birthday, password,
      telegram_id, vk_id, team;
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