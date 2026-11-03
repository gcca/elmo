#include <memory>
#include <regex>
#include <string>
#include <tuple>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common.hpp"
#include "elmo/testing/db.hpp"

namespace elmo::handling::common {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using Db = std::unique_ptr<sqlite3, int (*)(sqlite3 *)>;

Db Open() { return {elmo::core::db::Connect(), sqlite3_close_v2}; }

class IsIdTest
    : public ::testing::TestWithParam<std::tuple<std::string, bool>> {};

TEST_P(IsIdTest, Classifies) {
  const auto &[value, expected] = GetParam();
  EXPECT_EQ(IsId(value), expected) << value;
}

INSTANTIATE_TEST_SUITE_P(
    Values, IsIdTest,
    ::testing::Values(std::tuple{std::string("1"), true},
                      std::tuple{std::string("0042"), true},
                      std::tuple{std::string("123456789012345678"), true},
                      std::tuple{std::string("1234567890123456789"), false},
                      std::tuple{std::string(""), false},
                      std::tuple{std::string("-1"), false},
                      std::tuple{std::string("1x"), false},
                      std::tuple{std::string(" 1"), false}));

class IsCodeTest
    : public ::testing::TestWithParam<std::tuple<std::string, bool>> {};

TEST_P(IsCodeTest, Classifies) {
  const auto &[value, expected] = GetParam();
  EXPECT_EQ(IsCode(value), expected) << value;
}

INSTANTIATE_TEST_SUITE_P(
    Values, IsCodeTest,
    ::testing::Values(std::tuple{std::string("ad_space"), true},
                      std::tuple{std::string("a1_"), true},
                      std::tuple{std::string(64, 'a'), true},
                      std::tuple{std::string(65, 'a'), false},
                      std::tuple{std::string(""), false},
                      std::tuple{std::string("Ad"), false},
                      std::tuple{std::string("a-b"), false},
                      std::tuple{std::string("ñ"), false}));

TEST(Utf8LengthTest, CountsCodePoints) {
  EXPECT_EQ(Utf8Length(""), 0U);
  EXPECT_EQ(Utf8Length("abc"), 3U);
  EXPECT_EQ(Utf8Length("ñandú"), 5U);
  EXPECT_EQ(Utf8Length("€"), 1U);
  EXPECT_EQ(Utf8Length("🧸 x"), 3U);
}

TEST(TodayTest, IsIsoDate) {
  EXPECT_TRUE(std::regex_match(Today(), std::regex(R"(\d{4}-\d{2}-\d{2})")));
}

class CommonDbTest : public ::testing::Test {
protected:
  elmo::testing::TempDb temp_;
};

TEST_F(CommonDbTest, FetchRowsBindsTextAndReturnsColumnsAsText) {
  auto db = Open();
  const auto rows =
      FetchRows(db.get(), "SELECT ? AS a, 2 AS b, NULL AS c, 1.5 AS d", {"ñ"});
  ASSERT_EQ(rows.size(), 1U);
  EXPECT_THAT(rows.front(), ElementsAre(Pair("a", "ñ"), Pair("b", "2"),
                                        Pair("c", ""), Pair("d", "1.5")));
}

TEST_F(CommonDbTest, FetchRowsReturnsEmptyOnPrepareError) {
  auto db = Open();
  EXPECT_TRUE(FetchRows(db.get(), "SELECT FROM nowhere").empty());
}

TEST_F(CommonDbTest, ExecReportsInsertIdChangesAndConstraintKinds) {
  auto db = Open();
  const auto first =
      Exec(db.get(), "INSERT INTO party_client (name, ruc) VALUES (?, ?)",
           {std::string("A"), std::string("20999990001")});
  ASSERT_TRUE(first.ok());
  EXPECT_GT(first.last_insert_id, 0);
  EXPECT_EQ(first.changes, 1);

  const auto duplicate =
      Exec(db.get(), "INSERT INTO party_client (name, ruc) VALUES (?, ?)",
           {std::string("B"), std::string("20999990001")});
  EXPECT_FALSE(duplicate.ok());
  EXPECT_TRUE(duplicate.unique());
  EXPECT_FALSE(duplicate.foreign_key());

  const auto orphan = Exec(
      db.get(), "INSERT INTO party_contact (client_id, name) VALUES (?, ?)",
      {std::int64_t{999999}, std::string("X")});
  EXPECT_FALSE(orphan.ok());
  EXPECT_TRUE(orphan.foreign_key());
  EXPECT_FALSE(orphan.unique());

  const auto nulls =
      Exec(db.get(), "UPDATE party_client SET address = ? WHERE ruc = ?",
           {nullptr, std::string("20999990001")});
  EXPECT_FALSE(nulls.ok());

  const auto none =
      Exec(db.get(), "UPDATE party_client SET name = 'Z' WHERE id = ?",
           {std::int64_t{-1}});
  EXPECT_TRUE(none.ok());
  EXPECT_EQ(none.changes, 0);
}

TEST_F(CommonDbTest, ExecReportsPrepareError) {
  auto db = Open();
  EXPECT_FALSE(Exec(db.get(), "NOT SQL").ok());
}

TEST_F(CommonDbTest, RollbackDiscardsAndCommitKeeps) {
  auto db = Open();
  ASSERT_TRUE(Begin(db.get()));
  ASSERT_TRUE(Exec(db.get(),
                   "INSERT INTO party_client (name, ruc) VALUES ('R', "
                   "'20999990002')")
                  .ok());
  Rollback(db.get());
  EXPECT_EQ(temp_.Scalar("SELECT count(*) FROM party_client"), "0");

  ASSERT_TRUE(Begin(db.get()));
  ASSERT_TRUE(Exec(db.get(),
                   "INSERT INTO party_client (name, ruc) VALUES ('C', "
                   "'20999990003')")
                  .ok());
  ASSERT_TRUE(Commit(db.get()));
  EXPECT_EQ(temp_.Scalar("SELECT count(*) FROM party_client"), "1");
}

TEST_F(CommonDbTest, FetchClientsAndIssuersAreSortedByName) {
  temp_.Exec("INSERT INTO party_client (name, ruc) VALUES ('Beta', "
             "'20999990011'), ('Alfa', '20999990012');"
             "INSERT INTO party_issuer (name, ruc) VALUES ('Emisor', "
             "'20999990013');");
  auto db = Open();
  const auto clients = FetchClients(db.get());
  ASSERT_EQ(clients.size(), 2U);
  EXPECT_EQ(clients[0].at("name"), "Alfa");
  EXPECT_EQ(clients[1].at("name"), "Beta");
  EXPECT_THAT(clients,
              Each(AllOf(Contains(Key("id")), Contains(Key("ruc")),
                         Contains(Key("address")), Contains(Key("district")))));
  const auto issuers = FetchIssuers(db.get());
  ASSERT_EQ(issuers.size(), 1U);
  EXPECT_EQ(issuers[0].at("name"), "Emisor");
}

std::vector<std::string> Column(const Rows &rows, const std::string &key) {
  std::vector<std::string> out;
  for (const auto &row : rows)
    out.push_back(row.at(key));
  return out;
}

TEST_F(CommonDbTest, FormDefinitionKeepsActiveDefinitionsOfTheFamily) {
  temp_.LoadFixture();
  auto db = Open();

  const auto rent = FetchFormDefinition(db.get(), "rent");
  EXPECT_THAT(
      Column(rent.blocks, "code"),
      ElementsAre("campaign", "art_specs", "activation", "billing_extra"));
  EXPECT_THAT(Column(rent.fields, "code"), Not(Contains("legacy_code")));
  EXPECT_THAT(Column(rent.fields, "code"), Not(Contains("lead_days")));
  EXPECT_THAT(Column(rent.options, "code"), Not(Contains("paper")));
  EXPECT_THAT(Column(rent.options, "code"), Contains("vinyl"));

  const auto production = FetchFormDefinition(db.get(), "production");
  EXPECT_THAT(Column(production.blocks, "code"),
              UnorderedElementsAre("campaign", "art_specs", "technical",
                                   "billing_extra"));
  EXPECT_THAT(Column(production.fields, "code"), Contains("lead_days"));
  EXPECT_THAT(Column(production.fields, "code"), Not(Contains("promoters")));

  const auto scoped = temp_.Scalar(
      "SELECT id FROM rentorder_contract_kind WHERE code = 'ad_space'");
  for (const auto &b : rent.blocks) {
    if (b.at("code") == "art_specs")
      EXPECT_EQ(b.at("contract_kind_id"), scoped);
    if (b.at("code") == "campaign")
      EXPECT_EQ(b.at("contract_kind_id"), "");
  }
}

} // namespace
} // namespace elmo::handling::common
