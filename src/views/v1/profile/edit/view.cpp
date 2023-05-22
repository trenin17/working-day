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

using json = nlohmann::json;

namespace views::v1::profile::edit {

namespace {

class ProfileEditRequest {
 public:
  ProfileEditRequest(const std::string& body) {
    auto j = json::parse(body);

    if (j.contains("phone")) {
      phone = j["phone"];
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
  }

  std::optional<std::string> phone, email, birthday, password;
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
    const auto& user_id = ctx.GetData<std::string>("user_id");

    ProfileEditRequest request_body(request.RequestBody());

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day.employees "
        "SET phone = case when $2 is null then phone else $2 end, "
        "email = case when $3 is null then email else $3 end, "
        "birthday = case when $4 is null then birthday else $4 end, "
        "password = case when $5 is null then password else $5 end "
        "WHERE id = $1",
        user_id, request_body.phone, request_body.email, request_body.birthday, request_body.password);

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