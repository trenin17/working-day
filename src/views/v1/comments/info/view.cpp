#define V1_COMMENTS_INFO

#include "view.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include <definitions/all.hpp>

namespace views::v1::comments::info {

namespace {

class CommentsInfoHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-comments-info";

  CommentsInfoHandler(
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
    auto comment_id = request.GetArg("comment_id");

    if (comment_id.empty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kBadRequest);
      return ErrorMessage{"Missing comment_id parameter"}.ToJsonString();
    }

    // Check if comment exists and user has access to it
    // User has access if:
    // 1. User is the author of the comment
    // 2. Comment is linked to a task and user has access to that task
    //    (user is creator/assignee/observer of task OR user is in assigned_users_ids of project)
    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        R"(
        SELECT c.comment_id, c.author_id, c.data, c.documents_ids, c.created_ts, c.last_updated_ts
        FROM working_day_)" + company_id + R"(.comments c
        LEFT JOIN working_day_)" + company_id + R"(.task_comments tc
            ON tc.comment_id = c.comment_id
        LEFT JOIN working_day_)" + company_id + R"(.tracker_tasks t
            ON t.task_id = tc.task_id
        WHERE c.comment_id = $1
          AND (
            c.author_id = $2
            OR (
              tc.task_id IS NOT NULL
              AND (
                t.creator = $2
                OR t.assignee = $2
                OR EXISTS (
                    SELECT 1 FROM working_day_)" + company_id + R"(.tracker_task_observers o
                    WHERE o.task_id = t.task_id AND o.employee_id = $2
                )
                OR EXISTS (
                    SELECT 1 FROM working_day_)" + company_id + R"(.tracker_projects p
                    WHERE p.project_id = t.project_id AND p.creator = $2
                )
                OR EXISTS (
                    SELECT 1 FROM working_day_)" + company_id + R"(.tracker_project_assigned_users pau
                    WHERE pau.project_id = t.project_id AND pau.employee_id = $2
                )
              )
            )
          )
        )",
        comment_id, user_id);

    if (result.IsEmpty()) {
      request.GetHttpResponse().SetStatus(
          userver::server::http::HttpStatus::kNotFound);
      return ErrorMessage{"Comment not found"}.ToJsonString();
    }

    CommentItem response{result.AsSingleRow<CommentItem>(
        userver::storages::postgres::kRowTag)};

    return response.ToJsonString();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendCommentsInfo(userver::components::ComponentList& component_list) {
  component_list.Append<CommentsInfoHandler>();
}

}  // namespace views::v1::comments::info