#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <drogon/Session.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <pbkdf2_sha256.h>
#include <sqlite3.h>
#include <trantor/utils/Logger.h>

#include "elmo/core/db.hpp"

namespace elmo::handling::auth::utils {

inline constexpr std::string_view kSignInPath = "/elmo/auth/signin";
inline constexpr std::string_view kHomePath = "/elmo/home";

inline constexpr std::string_view kUsername = "username";
inline constexpr std::string_view kName = "name";
inline constexpr std::string_view kOAuthState = "oauth_state";

struct User {
  std::uint64_t id = 0;
  std::string username;
  std::string name;
};

inline void SessionSet(const drogon::SessionPtr &session, std::string_view key,
                       std::string value) {
  const std::string k(key);
  session->erase(k);
  session->insert(k, std::move(value));
}

[[nodiscard]] inline std::string SessionGet(const drogon::SessionPtr &session,
                                            std::string_view key) {
  if (!session)
    return {};
  return session->getOptional<std::string>(std::string(key)).value_or("");
}

[[nodiscard]] inline std::string RandomHex(std::size_t bytes) {
  std::string raw(bytes, '\0');
  if (!drogon::utils::secureRandomBytes(raw.data(), raw.size()))
    return {};
  return drogon::utils::binaryStringToHex(
      reinterpret_cast<const unsigned char *>(raw.data()), raw.size(), true);
}

[[nodiscard]] inline bool VerifyPassword(const std::string &password,
                                         const std::string &stored) {
  const auto p1 = stored.find('$');
  if (p1 == std::string::npos)
    return false;
  const auto p2 = stored.find('$', p1 + 1);
  if (p2 == std::string::npos)
    return false;
  const auto p3 = stored.find('$', p2 + 1);
  if (p3 == std::string::npos)
    return false;

  if (stored.substr(0, p1) != "pbkdf2_sha256")
    return false;

  std::uint32_t rounds = 0;
  try {
    rounds = static_cast<std::uint32_t>(
        std::stoul(stored.substr(p1 + 1, p2 - p1 - 1)));
  } catch (...) {
    return false;
  }
  const std::string salt = stored.substr(p2 + 1, p3 - p2 - 1);
  const std::string hex_dk = stored.substr(p3 + 1);

  constexpr std::uint32_t dklen = SHA256_DIGESTLEN;
  if (hex_dk.size() != dklen * 2)
    return false;

  std::uint8_t dk[dklen];
  HMAC_SHA256_CTX ctx;
  pbkdf2_sha256(&ctx, reinterpret_cast<const std::uint8_t *>(password.data()),
                static_cast<std::uint32_t>(password.size()),
                reinterpret_cast<const std::uint8_t *>(salt.data()),
                static_cast<std::uint32_t>(salt.size()), rounds, dk, dklen);

  unsigned char diff = 0;
  for (std::uint32_t i = 0; i < dklen; ++i) {
    char buf[3];
    std::snprintf(buf, sizeof(buf), "%02x", dk[i]);
    diff |= static_cast<unsigned char>(hex_dk[2 * i] ^ buf[0]);
    diff |= static_cast<unsigned char>(hex_dk[2 * i + 1] ^ buf[1]);
  }
  return diff == 0;
}

[[nodiscard]] inline std::optional<User>
Authenticate(sqlite3 *db, const std::string &username,
             const std::string &password) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT id, password, name
  FROM auth_user
 WHERE username = ?
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[auth] query error: " << sqlite3_errmsg(db);
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

  std::optional<User> user;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto stored = core::db::ColumnText(stmt, 1);
    if (VerifyPassword(password, stored)) {
      user = User{
          .id = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0)),
          .username = username,
          .name = core::db::ColumnText(stmt, 2),
      };
    }
  }

  sqlite3_finalize(stmt);
  return user;
}

inline void TouchLastLoggedIn(sqlite3 *db, std::uint64_t user_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql =
      "UPDATE auth_user SET last_logged_in = unixepoch() WHERE id = ?";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[auth] update error: " << sqlite3_errmsg(db);
    return;
  }
  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(user_id));
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

[[nodiscard]] inline std::optional<User>
UpsertOAuthUser(sqlite3 *db, const std::string &username,
                const std::string &name, const std::string &email) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO auth_user (username, password, name, email, last_logged_in)
VALUES (?, '!', ?, ?, unixepoch())
ON CONFLICT(username) DO UPDATE SET
  last_logged_in = unixepoch(),
  name  = excluded.name,
  email = excluded.email
RETURNING id, name
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[auth/otsc] upsert error: " << sqlite3_errmsg(db);
    return std::nullopt;
  }

  const std::string display = name.empty() ? username : name;
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, display.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_STATIC);

  std::optional<User> user;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    user = User{
        .id = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0)),
        .username = username,
        .name = core::db::ColumnText(stmt, 1),
    };
  } else {
    LOG_DEBUG << "[auth/otsc] upsert error: " << sqlite3_errmsg(db);
  }

  sqlite3_finalize(stmt);
  return user;
}

[[nodiscard]] inline std::string UrlEncode(std::string_view s) {
  std::string out;
  for (const unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      out += static_cast<char>(c);
    else
      out += std::format("%{:02X}", c);
  }
  return out;
}

[[nodiscard]] inline Json::Value DecodeJwtPayload(const std::string &token) {
  const auto dot1 = token.find('.');
  if (dot1 == std::string::npos)
    return {};
  const auto dot2 = token.find('.', dot1 + 1);
  if (dot2 == std::string::npos)
    return {};

  std::string payload = token.substr(dot1 + 1, dot2 - dot1 - 1);
  std::ranges::replace(payload, '-', '+');
  std::ranges::replace(payload, '_', '/');
  while (payload.size() % 4)
    payload += '=';

  const std::string decoded = drogon::utils::base64Decode(payload);
  Json::Value result;
  Json::CharReaderBuilder builder;
  std::string errs;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  if (!reader->parse(decoded.data(), decoded.data() + decoded.size(), &result,
                     &errs))
    return {};
  return result;
}

} // namespace elmo::handling::auth::utils
