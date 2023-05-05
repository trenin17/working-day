#pragma once

/// [user info cache]
#include <vector>

#include <userver/cache/base_postgres_cache.hpp>
#include <userver/crypto/algorithm.hpp>
#include <userver/server/auth/user_auth_info.hpp>
#include <userver/storages/postgres/io/array_types.hpp>

namespace samples::pg {

struct UserDbInfo {
  userver::server::auth::UserAuthInfo::Ticket token;
  std::string user_id;
  std::vector<std::string> scopes;
};

struct AuthCachePolicy {
  static constexpr std::string_view kName = "auth-pg-cache";

  using ValueType = UserDbInfo;
  static constexpr auto kKeyMember = &UserDbInfo::token;
  static constexpr const char* kQuery =
      "SELECT token, user_id, scopes FROM working_day.auth_tokens";
  static constexpr const char* kUpdatedField = "updated";
  using UpdatedFieldType = userver::storages::postgres::TimePointTz;

  // Using crypto::algorithm::StringsEqualConstTimeComparator to avoid timing
  // attack at find(token).
  using CacheContainer = std::unordered_map<
      userver::server::auth::UserAuthInfo::Ticket, UserDbInfo,
      std::hash<userver::server::auth::UserAuthInfo::Ticket>,
      userver::crypto::algorithm::StringsEqualConstTimeComparator>;
};

using AuthCache = userver::components::PostgreCache<AuthCachePolicy>;

}  // namespace samples::pg
   /// [user info cache]