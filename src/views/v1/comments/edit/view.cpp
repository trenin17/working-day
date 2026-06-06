#define V1_COMMENTS_EDIT

#include "view.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>

namespace views::v1::comments::edit {

namespace {

class CommentsEditHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-comments-edit";

  CommentsEditHandler(
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
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& user_id = ctx.GetData<std::string>("user_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    CommentEditRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto comment_id = request.GetArg("comment_id");

    if (comment_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing comment_id parameter"}.ToJsonString();
    }

    auto check_result = pg_cluster_->Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      "SELECT author_id FROM working_day_" + company_id + ".comments WHERE comment_id = $1",
      comment_id);

    if (check_result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Comment not found"}.ToJsonString();
    }

    auto author_id = check_result.AsSingleRow<std::string>();

    if (author_id != user_id) {
      request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kForbidden);
      return ErrorMessage{"You are not the author of this comment"}.ToJsonString();
    }

    pg_cluster_->Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      "UPDATE working_day_" + company_id + ".comments SET data = $1, last_updated_ts = NOW() WHERE comment_id = $2",
      request_body.data, comment_id);

    // TODO: add project, task last_updated_ts update ?

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendCommentsEdit(userver::components::ComponentList& component_list) {
  component_list.Append<CommentsEditHandler>();
}

}  // namespace views::v1::comments::edit