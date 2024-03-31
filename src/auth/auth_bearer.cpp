/// [auth checker declaration]
#include "auth_bearer.hpp"
#include "user_info_cache.hpp"

#include <algorithm>

namespace auth {

class AuthCheckerBearer final
    : public userver::server::handlers::auth::AuthCheckerBase {
 public:
  using AuthCheckResult = userver::server::handlers::auth::AuthCheckResult;

  AuthCheckerBearer(
      const AuthCache& auth_cache,
      std::vector<userver::server::auth::UserScope> required_scopes,
      userver::storages::postgres::ClusterPtr pg_cluster)
      : auth_cache_(auth_cache),
        required_scopes_(std::move(required_scopes)),
        pg_cluster_(pg_cluster) {}

  [[nodiscard]] AuthCheckResult CheckAuth(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& request_context) const override;

  [[nodiscard]] bool SupportsUserAuth() const noexcept override { return true; }

 private:
  const AuthCache& auth_cache_;
  const std::vector<userver::server::auth::UserScope> required_scopes_;
  userver::storages::postgres::ClusterPtr pg_cluster_;
};
/// [auth checker declaration]

/// [auth checker definition 1]
AuthCheckerBearer::AuthCheckResult AuthCheckerBearer::CheckAuth(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& request_context) const {
  const auto& auth_value = request.GetHeader("Authorization");
  if (auth_value.empty()) {
    return AuthCheckResult{
        AuthCheckResult::Status::kTokenNotFound,
        {},
        "Empty 'Authorization' header",
        userver::server::handlers::HandlerErrorCode::kUnauthorized};
  }
  /// [auth checker definition 1]

  /// [auth checker definition 2]
  const auto bearer_sep_pos = auth_value.find(' ');
  if (bearer_sep_pos == std::string::npos ||
      std::string_view{auth_value.data(), bearer_sep_pos} != "Bearer") {
    return AuthCheckResult{
        AuthCheckResult::Status::kTokenNotFound,
        {},
        "'Authorization' header should have 'Bearer some-token' format",
        userver::server::handlers::HandlerErrorCode::kUnauthorized};
  }
  /// [auth checker definition 2]

  /// [auth checker definition 3]
  const userver::server::auth::UserAuthInfo::Ticket token{auth_value.data() +
                                                          bearer_sep_pos + 1};
  const auto cache_snapshot = auth_cache_.Get();

  auto it = cache_snapshot->find(token);
  std::optional<UserDbInfo> not_cached_info;
  if (it == cache_snapshot->end()) {
    LOG_INFO() << "Trying to search token in database";
    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        static_cast<std::string>(AuthCachePolicy::kQuery) + " WHERE token = $1",
        token);
    if (result.IsEmpty()) {
      LOG_INFO() << "Search of token in database was unsuccessful";
      return AuthCheckResult{AuthCheckResult::Status::kForbidden};
    }
    LOG_INFO() << "Search of token in database was successful";
    not_cached_info =
        result.AsSingleRow<UserDbInfo>(userver::storages::postgres::kRowTag);
  }
  /// [auth checker definition 3]

  /// [auth checker definition 4]
  UserDbInfo info;
  if (not_cached_info.has_value()) {
    info = not_cached_info.value();
  } else {
    info = it->second;
  }

  std::set<std::string> user_scopes(info.scopes.begin(), info.scopes.end());
  std::set<std::string> required_scopes;
  for (auto& scope: required_scopes_) {
    required_scopes.insert(scope.GetValue());
  }

  std::vector<std::string> intersection;
  std::set_intersection(user_scopes.begin(), user_scopes.end(),
                        required_scopes.begin(), required_scopes.end(),
                        std::back_inserter(intersection));

  if (intersection.empty()) {
    return AuthCheckResult{AuthCheckResult::Status::kForbidden,
                            {},
                            "Not enough permissions"};
  }
  /// [auth checker definition 4]

  /// [auth checker definition 5]
  request_context.SetData("user_id", info.user_id);
  if (std::find(info.scopes.begin(), info.scopes.end(), "admin") !=
      info.scopes.end()) {
    request_context.SetData("is_admin", true);
  } else {
    request_context.SetData("is_admin", false);
  }
  request_context.SetData("company_id", info.company_id);
  return {};
}
/// [auth checker definition 5]

/// [auth checker factory definition]
userver::server::handlers::auth::AuthCheckerBasePtr CheckerFactory::operator()(
    const ::userver::components::ComponentContext& context,
    const userver::server::handlers::auth::HandlerAuthConfig& auth_config,
    const userver::server::handlers::auth::AuthCheckerSettings&) const {
  auto scopes = auth_config["scopes"].As<userver::server::auth::UserScopes>({});
  const auto& auth_cache = context.FindComponent<AuthCache>();
  return std::make_shared<AuthCheckerBearer>(
      auth_cache, std::move(scopes),
      context.FindComponent<userver::components::Postgres>("key-value")
          .GetCluster());
}
/// [auth checker factory definition]

}  // namespace auth