#define V1_COMMENTS_ADD

#include "view.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>

namespace views::v1::comments::add {

namespace {

class CommentsAddHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-comments-add";

  CommentsAddHandler(
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

    CommentAddRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    auto comment_id = userver::utils::generators::GenerateUuid();

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id + ".comments(comment_id, author_id, data, created_ts, last_updated_ts) "
        "VALUES($1, $2, $3, NOW(), NOW())",
        comment_id,
        user_id,
        request_body.data);

    if (request_body.task_id) {
      auto check_result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT 1 FROM working_day_" + company_id + ".tracker_tasks WHERE task_id = $1",
        request_body.task_id);

      if (check_result.IsEmpty()) {
        request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kNotFound);
        return ErrorMessage{"Task not found"}.ToJsonString();
      }

      pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id + ".task_comments(task_id, comment_id) "
        "VALUES($1, $2)",
        request_body.task_id,
        comment_id);
    }

    CommentAddResponse response;
    response.comment_id = comment_id;

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendCommentsAdd(userver::components::ComponentList& component_list) {
  component_list.Append<CommentsAddHandler>();
}

}  // namespace views::v1::comments::add