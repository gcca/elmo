#include <filesystem>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "db.hpp"
#include "elmo/core/conf.hpp"
#include "elmo/testing/db.hpp"

namespace elmo::core::db {
namespace {

using Db = std::unique_ptr<sqlite3, int (*)(sqlite3 *)>;

std::string Pragma(sqlite3 *db, const char *sql) {
  sqlite3_stmt *stmt = nullptr;
  std::string value;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK &&
      sqlite3_step(stmt) == SQLITE_ROW)
    value = ColumnText(stmt, 0);
  sqlite3_finalize(stmt);
  return value;
}

TEST(DbTest, ConnectEnablesForeignKeysAndWal) {
  const elmo::testing::TempDb temp;
  Db db{Connect(), sqlite3_close_v2};
  ASSERT_NE(db, nullptr);
  EXPECT_EQ(Pragma(db.get(), "PRAGMA foreign_keys"), "1");
  EXPECT_EQ(Pragma(db.get(), "PRAGMA journal_mode"), "wal");
}

TEST(DbTest, ConnectCreatesMissingFile) {
  const elmo::testing::TempDb temp;
  const auto previous = conf::settings.DB_PATH;
  const auto path = temp.path().string() + "-new";
  conf::settings.DB_PATH = path;
  {
    Db db{Connect(), sqlite3_close_v2};
    EXPECT_NE(db, nullptr);
  }
  EXPECT_TRUE(std::filesystem::exists(path));
  for (const auto *suffix : {"", "-wal", "-shm"})
    std::filesystem::remove(path + suffix);
  conf::settings.DB_PATH = previous;
}

TEST(DbTest, ConnectReadOnlyRejectsWrites) {
  const elmo::testing::TempDb temp;
  Db db{ConnectRO(), sqlite3_close_v2};
  ASSERT_NE(db, nullptr);
  EXPECT_EQ(Pragma(db.get(), "PRAGMA query_only"), "1");
  EXPECT_NE(sqlite3_exec(db.get(),
                         "INSERT INTO party_client (name, ruc) "
                         "VALUES ('x', '20000000001')",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
}

TEST(DbTest, ConnectReadOnlyFailsForMissingFile) {
  const auto previous = conf::settings.DB_PATH;
  conf::settings.DB_PATH =
      (std::filesystem::temp_directory_path() / "elmo-test-missing.db")
          .string();
  std::filesystem::remove(conf::settings.DB_PATH);
  EXPECT_EQ(ConnectRO(), nullptr);
  EXPECT_FALSE(std::filesystem::exists(conf::settings.DB_PATH));
  conf::settings.DB_PATH = previous;
}

TEST(DbTest, EmptyPathFails) {
  const auto previous = conf::settings.DB_PATH;
  conf::settings.DB_PATH.clear();
  EXPECT_EQ(Connect(), nullptr);
  EXPECT_EQ(ConnectRO(), nullptr);
  conf::settings.DB_PATH = previous;
}

TEST(DbTest, ColumnTextMapsNullToEmpty) {
  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open(":memory:", &raw), SQLITE_OK);
  Db db{raw, sqlite3_close_v2};
  sqlite3_stmt *stmt = nullptr;
  ASSERT_EQ(
      sqlite3_prepare_v2(db.get(), "SELECT NULL, 'ñ'", -1, &stmt, nullptr),
      SQLITE_OK);
  ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
  EXPECT_EQ(ColumnText(stmt, 0), "");
  EXPECT_EQ(ColumnText(stmt, 1), "ñ");
  sqlite3_finalize(stmt);
}

} // namespace
} // namespace elmo::core::db
