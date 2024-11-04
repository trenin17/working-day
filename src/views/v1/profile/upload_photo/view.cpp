#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "utils/s3_presigned_links.hpp"

using json = nlohmann::json;

namespace views::v1::profile::upload_photo {

namespace {

class UploadPhotoResponse {
 public:
  std::string ToJSON() {
    json j;
    j["url"] = url;

    return j.dump();
  }

  std::string url;
};

class ProfileUploadPhotoHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-profile-upload-photo";

  ProfileUploadPhotoHandler(
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

    auto photo_id = userver::utils::generators::GenerateUuid();
    auto upload_link = utils::s3_presigned_links::GeneratePhotoPresignedLink(
        photo_id, utils::s3_presigned_links::Upload);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "UPDATE working_day_" + company_id +
            ".employees "
            "SET photo_link = $2 "
            "WHERE id = $1",
        user_id, photo_id);

    UploadPhotoResponse response{upload_link};
    return response.ToJSON();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendProfileUploadPhoto(
    userver::components::ComponentList& component_list) {
  component_list.Append<ProfileUploadPhotoHandler>();
}

}  // namespace views::v1::profile::upload_photo