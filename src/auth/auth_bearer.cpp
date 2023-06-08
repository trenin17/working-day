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
      std::vector<userver::server::auth::UserScope> required_scopes)
      : auth_cache_(auth_cache), required_scopes_(std::move(required_scopes)) {}

  [[nodiscard]] AuthCheckResult CheckAuth(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& request_context) const override;

  [[nodiscard]] bool SupportsUserAuth() const noexcept override { return true; }

 private:
  const AuthCache& auth_cache_;
  const std::vector<userver::server::auth::UserScope> required_scopes_;
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
  if (it == cache_snapshot->end()) {
    return AuthCheckResult{AuthCheckResult::Status::kForbidden};
  }
  /// [auth checker definition 3]

  /// [auth checker definition 4]
  const UserDbInfo& info = it->second;
  for (const auto& scope : required_scopes_) {
    if (std::find(info.scopes.begin(), info.scopes.end(), scope.GetValue()) ==
        info.scopes.end()) {
      return AuthCheckResult{AuthCheckResult::Status::kForbidden,
                             {},
                             "No '" + scope.GetValue() + "' permission"};
    }
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
  return std::make_shared<AuthCheckerBearer>(auth_cache, std::move(scopes));
}
/// [auth checker factory definition]

}  // namespace auth