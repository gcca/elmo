#include "db.hpp"

#include <atomic>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include <unistd.h>

#include "elmo/core/conf.hpp"

namespace elmo::testing {

namespace {

using Db = std::unique_ptr<sqlite3, int (*)(sqlite3 *)>;

Db Open(const std::filesystem::path &path) {
  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(path.c_str(), &db,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                      nullptr) != SQLITE_OK) {
    sqlite3_close_v2(db);
    throw std::runtime_error("cannot open " + path.string());
  }
  sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
  return {db, sqlite3_close_v2};
}

void Run(sqlite3 *db, const std::string &sql) {
  char *err = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    const std::string message = err ? err : "unknown error";
    sqlite3_free(err);
    throw std::runtime_error("sql error: " + message);
  }
}

std::string MigrationUp() {
  const auto text = ReadSource("db/migrations/20261103133615_init.sql");
  const auto up = text.find("-- migrate:up");
  const auto down = text.find("-- migrate:down");
  if (up == std::string::npos || down == std::string::npos)
    throw std::runtime_error("init migration markers not found");
  return text.substr(up, down - up);
}

} // namespace

std::string ReadSource(std::string_view relative) {
  const auto path = std::filesystem::path(ELMO_SOURCE_DIR) / relative;
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("cannot read " + path.string());
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

TempDb::TempDb() : previous_(core::conf::settings.DB_PATH) {
  static std::atomic<int> counter{0};
  path_ = std::filesystem::temp_directory_path() /
          ("elmo-test-" + std::to_string(::getpid()) + "-" +
           std::to_string(counter++) + ".db");
  std::filesystem::remove(path_);
  auto db = Open(path_);
  Run(db.get(), MigrationUp());
  core::conf::settings.DB_PATH = path_.string();
}

TempDb::~TempDb() {
  core::conf::settings.DB_PATH = previous_;
  for (const auto *suffix : {"", "-wal", "-shm"}) {
    std::error_code ec;
    std::filesystem::remove(path_.string() + suffix, ec);
  }
}

void TempDb::Exec(const std::string &sql) const {
  auto db = Open(path_);
  Run(db.get(), sql);
}

void TempDb::LoadFixture() const {
  Exec(ReadSource("db/fixtures/sample-data.sql"));
}

std::string TempDb::Scalar(const std::string &sql) const {
  auto db = Open(path_);
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
    throw std::runtime_error("prepare error: " +
                             std::string(sqlite3_errmsg(db.get())));
  std::string value;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto *text = sqlite3_column_text(stmt, 0);
    value = text ? reinterpret_cast<const char *>(text) : "";
  }
  sqlite3_finalize(stmt);
  return value;
}

} // namespace elmo::testing
