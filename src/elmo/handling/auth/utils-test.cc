#include <cstdint>
#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elmo/testing/db.hpp"
#include "utils.hpp"

namespace elmo::handling::auth::utils {
namespace {

using ::testing::ContainsRegex;
using ::testing::Field;
using ::testing::MatchesRegex;
using ::testing::Optional;

using Db = std::unique_ptr<sqlite3, int (*)(sqlite3 *)>;

std::string Hash(const std::string &password, const std::string &salt,
                 std::uint32_t rounds) {
  std::uint8_t dk[SHA256_DIGESTLEN];
  HMAC_SHA256_CTX ctx;
  pbkdf2_sha256(&ctx, reinterpret_cast<const std::uint8_t *>(password.data()),
                static_cast<std::uint32_t>(password.size()),
                reinterpret_cast<const std::uint8_t *>(salt.data()),
                static_cast<std::uint32_t>(salt.size()), rounds, dk,
                SHA256_DIGESTLEN);
  return std::format("pbkdf2_sha256${}${}${}", rounds, salt,
                     drogon::utils::binaryStringToHex(dk, sizeof(dk), true));
}

TEST(VerifyPasswordTest, AcceptsMatchingPassword) {
  EXPECT_TRUE(VerifyPassword("s3cret", Hash("s3cret", "salt", 1000)));
  EXPECT_TRUE(VerifyPassword("ñandú", Hash("ñandú", "x", 10)));
}

TEST(VerifyPasswordTest, RejectsWrongPasswordAndMalformedHashes) {
  const auto stored = Hash("s3cret", "salt", 1000);
  EXPECT_FALSE(VerifyPassword("S3cret", stored));
  EXPECT_FALSE(VerifyPassword("", stored));
  EXPECT_FALSE(VerifyPassword("s3cret", "!"));
  EXPECT_FALSE(VerifyPassword("s3cret", ""));
  EXPECT_FALSE(VerifyPassword("s3cret", "pbkdf2_sha256$1000$salt"));
  EXPECT_FALSE(VerifyPassword("s3cret", "md5$1000$salt$abcd"));
  EXPECT_FALSE(VerifyPassword("s3cret", "pbkdf2_sha256$many$salt$abcd"));
  EXPECT_FALSE(VerifyPassword("s3cret", stored.substr(0, stored.size() - 2)));
}

TEST(RandomHexTest, ReturnsLowercaseHexOfRequestedSize) {
  const auto a = RandomHex(32);
  EXPECT_THAT(a, MatchesRegex("[0-9a-f]{64}"));
  EXPECT_NE(a, RandomHex(32));
  EXPECT_EQ(RandomHex(0), "");
}

TEST(SessionTest, GetOnMissingSessionIsEmpty) {
  EXPECT_EQ(SessionGet(nullptr, kUsername), "");
}

TEST(UrlEncodeTest, EncodesReservedAndUtf8Bytes) {
  EXPECT_EQ(UrlEncode("aZ09-_.~"), "aZ09-_.~");
  EXPECT_EQ(UrlEncode("a b&c=d/"), "a%20b%26c%3Dd%2F");
  EXPECT_EQ(UrlEncode("ñ"), "%C3%B1");
}

TEST(DecodeJwtPayloadTest, DecodesBase64UrlPayload) {
  const auto payload =
      drogon::utils::base64Encode(R"({"name":"Ana Ñ","upn":"a@b.c"})");
  std::string url = payload;
  std::ranges::replace(url, '+', '-');
  std::ranges::replace(url, '/', '_');
  std::erase(url, '=');
  const auto claims = DecodeJwtPayload("h." + url + ".s");
  EXPECT_EQ(claims["name"].asString(), "Ana Ñ");
  EXPECT_EQ(claims["upn"].asString(), "a@b.c");
}

TEST(DecodeJwtPayloadTest, ReturnsNullForInvalidTokens) {
  EXPECT_TRUE(DecodeJwtPayload("").isNull());
  EXPECT_TRUE(DecodeJwtPayload("only.one").isNull());
  EXPECT_TRUE(DecodeJwtPayload("a.bm90IGpzb24.c").isNull());
}

class AuthDbTest : public ::testing::Test {
protected:
  void SetUp() override {
    temp_.Exec("INSERT INTO auth_user (username, password, name) VALUES "
               "('ana', '" +
               Hash("pw", "salt", 50) + "', 'Ana'), ('oauth', '!', 'OAuth')");
  }

  Db Open() const { return {core::db::Connect(), sqlite3_close_v2}; }

  elmo::testing::TempDb temp_;
};

TEST_F(AuthDbTest, AuthenticateReturnsUserOnlyForValidCredentials) {
  auto db = Open();
  EXPECT_THAT(Authenticate(db.get(), "ana", "pw"),
              Optional(AllOf(Field(&User::username, "ana"),
                             Field(&User::name, "Ana"))));
  EXPECT_EQ(Authenticate(db.get(), "ana", "nope"), std::nullopt);
  EXPECT_EQ(Authenticate(db.get(), "ghost", "pw"), std::nullopt);
  EXPECT_EQ(Authenticate(db.get(), "oauth", "!"), std::nullopt);
}

TEST_F(AuthDbTest, TouchLastLoggedInStoresEpochSeconds) {
  auto db = Open();
  const auto user = Authenticate(db.get(), "ana", "pw");
  ASSERT_TRUE(user);
  TouchLastLoggedIn(db.get(), user->id);
  EXPECT_EQ(temp_.Scalar("SELECT typeof(last_logged_in) FROM auth_user "
                         "WHERE username = 'ana'"),
            "integer");
  EXPECT_EQ(temp_.Scalar("SELECT abs(unixepoch() - last_logged_in) < 5 "
                         "FROM auth_user WHERE username = 'ana'"),
            "1");
}

TEST_F(AuthDbTest, UpsertOAuthUserInsertsThenUpdates) {
  auto db = Open();
  const auto created = UpsertOAuthUser(db.get(), "new@x.pe", "", "new@x.pe");
  ASSERT_TRUE(created);
  EXPECT_EQ(created->name, "new@x.pe");
  EXPECT_EQ(temp_.Scalar("SELECT password FROM auth_user WHERE username = "
                         "'new@x.pe'"),
            "!");
  const auto updated =
      UpsertOAuthUser(db.get(), "new@x.pe", "Nuevo", "otro@x.pe");
  ASSERT_TRUE(updated);
  EXPECT_EQ(updated->id, created->id);
  EXPECT_EQ(updated->name, "Nuevo");
  EXPECT_EQ(temp_.Scalar("SELECT email FROM auth_user WHERE username = "
                         "'new@x.pe'"),
            "otro@x.pe");
}

} // namespace
} // namespace elmo::handling::auth::utils
