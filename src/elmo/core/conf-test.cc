#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "conf.hpp"

namespace elmo::core::conf {
namespace {

constexpr const char *kVars[] = {"APPNAME",     "DB_PATH",
                                 "OTSC_CLIENT", "OTSC_SECRET",
                                 "OTSC_TENANT", "OTSC_REDIRECT_URI"};

class ConfTest : public ::testing::Test {
protected:
  void SetUp() override {
    saved_settings_ = settings;
    for (const auto *name : kVars) {
      const auto *value = std::getenv(name);
      saved_env_[name] =
          value ? std::optional<std::string>(value) : std::nullopt;
      ::unsetenv(name);
    }
    settings = Settings{};
    dir_ = std::filesystem::temp_directory_path() / "elmo-conf-test";
    std::filesystem::remove_all(dir_);
  }

  void TearDown() override {
    for (const auto &[name, value] : saved_env_) {
      if (value)
        ::setenv(name.c_str(), value->c_str(), 1);
      else
        ::unsetenv(name.c_str());
    }
    settings = saved_settings_;
    std::filesystem::remove_all(dir_);
  }

  Settings saved_settings_;
  std::map<std::string, std::optional<std::string>> saved_env_;
  std::filesystem::path dir_;
};

TEST_F(ConfTest, DefaultsWithoutEnvironment) {
  ::setenv("DB_PATH", (dir_ / "elmo.db").c_str(), 1);
  Load();
  EXPECT_EQ(settings.APPNAME, "Elmo");
  EXPECT_EQ(settings.OTSC_CLIENT, "");
  EXPECT_EQ(settings.OTSC_SECRET, "");
  EXPECT_EQ(settings.OTSC_TENANT, "organizations");
  EXPECT_EQ(settings.OTSC_REDIRECT_URI,
            "http://localhost:8000/elmo/auth/otsc/callback");
}

TEST_F(ConfTest, EnvironmentOverridesEverySetting) {
  ::setenv("APPNAME", "Notas", 1);
  ::setenv("DB_PATH", (dir_ / "nested" / "x.db").c_str(), 1);
  ::setenv("OTSC_CLIENT", "client", 1);
  ::setenv("OTSC_SECRET", "secret", 1);
  ::setenv("OTSC_TENANT", "tenant", 1);
  ::setenv("OTSC_REDIRECT_URI", "https://example.com/cb", 1);
  Load();
  EXPECT_EQ(settings.APPNAME, "Notas");
  EXPECT_EQ(settings.DB_PATH, (dir_ / "nested" / "x.db").string());
  EXPECT_EQ(settings.OTSC_CLIENT, "client");
  EXPECT_EQ(settings.OTSC_SECRET, "secret");
  EXPECT_EQ(settings.OTSC_TENANT, "tenant");
  EXPECT_EQ(settings.OTSC_REDIRECT_URI, "https://example.com/cb");
}

TEST_F(ConfTest, EmptyAppnameKeepsDefault) {
  ::setenv("APPNAME", "", 1);
  ::setenv("DB_PATH", (dir_ / "elmo.db").c_str(), 1);
  Load();
  EXPECT_EQ(settings.APPNAME, "Elmo");
}

TEST_F(ConfTest, CreatesDatabaseParentDirectory) {
  const auto db = dir_ / "a" / "b" / "elmo.db";
  ::setenv("DB_PATH", db.c_str(), 1);
  Load();
  EXPECT_TRUE(std::filesystem::is_directory(db.parent_path()));
  EXPECT_FALSE(std::filesystem::exists(db));
}

} // namespace
} // namespace elmo::core::conf
