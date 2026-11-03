#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <drogon/HttpResponse.h>
#include <sqlite3.h>

#include "elmo/core/db.hpp"

namespace elmo::handling::common {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
using elmo::core::db::Row;
using elmo::core::db::Rows;

using Bind = std::variant<std::nullptr_t, std::int64_t, std::string>;

struct ExecResult {
  int rc = SQLITE_ERROR;
  int extended_rc = SQLITE_ERROR;
  std::int64_t last_insert_id = 0;
  int changes = 0;

  [[nodiscard]] bool ok() const { return rc == SQLITE_DONE || rc == SQLITE_OK; }
  [[nodiscard]] bool unique() const {
    return extended_rc == SQLITE_CONSTRAINT_UNIQUE ||
           extended_rc == SQLITE_CONSTRAINT_PRIMARYKEY;
  }
  [[nodiscard]] bool foreign_key() const {
    return extended_rc == SQLITE_CONSTRAINT_FOREIGNKEY;
  }
};

struct FormDefinition {
  Rows blocks;
  Rows fields;
  Rows options;
};

[[nodiscard]] Rows FetchRows(sqlite3 *db, std::string_view sql,
                             const std::vector<std::string> &binds = {});

[[nodiscard]] ExecResult Exec(sqlite3 *db, std::string_view sql,
                              const std::vector<Bind> &binds = {});

[[nodiscard]] bool Begin(sqlite3 *db);

[[nodiscard]] bool Commit(sqlite3 *db);

void Rollback(sqlite3 *db);

[[nodiscard]] Rows FetchClients(sqlite3 *db);

[[nodiscard]] Rows FetchIssuers(sqlite3 *db);

[[nodiscard]] FormDefinition FetchFormDefinition(sqlite3 *db,
                                                 std::string_view family);

[[nodiscard]] bool IsId(std::string_view value);

[[nodiscard]] bool IsCode(std::string_view value);

[[nodiscard]] std::string Today();

inline constexpr std::size_t kHelpTextMax = 500;

[[nodiscard]] std::size_t Utf8Length(std::string_view value);

void ServiceUnavailable(const Callback &callback);

} // namespace elmo::handling::common
