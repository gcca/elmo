#include "db.hpp"

#include <trantor/utils/Logger.h>

#include "elmo/core/conf.hpp"

namespace {

[[nodiscard]] inline sqlite3 *Fail(sqlite3 *db, const char *what) {
  LOG_ERROR << "[db] " << what << ": " << sqlite3_errmsg(db);
  sqlite3_close_v2(db);
  return nullptr;
}

[[nodiscard]] inline sqlite3 *Open(int flags) {
  const std::string &path = elmo::core::conf::settings.DB_PATH;
  if (path.empty()) {
    LOG_ERROR << "[db] empty DB_PATH";
    return nullptr;
  }

  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(path.c_str(), &db, flags, nullptr) != SQLITE_OK)
    return Fail(db, "open error");

  sqlite3_busy_timeout(db, 5000);
  return db;
}

[[nodiscard]] inline sqlite3 *ApplyPragmas(sqlite3 *db, const char *sql) {
  if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
    return Fail(db, "pragma error");
  return db;
}

} // namespace

namespace elmo::core::db {

sqlite3 *Connect() {
  sqlite3 *db =
      Open(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
  if (!db)
    return nullptr;
  return ApplyPragmas(db, "PRAGMA journal_mode=WAL;"
                          "PRAGMA synchronous=NORMAL;"
                          "PRAGMA foreign_keys=ON;");
}

sqlite3 *ConnectRO() {
  sqlite3 *db = Open(SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX);
  if (!db)
    return nullptr;
  return ApplyPragmas(db, "PRAGMA foreign_keys=ON;"
                          "PRAGMA query_only=ON;");
}

} // namespace elmo::core::db
