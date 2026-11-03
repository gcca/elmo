#include "conf.hpp"

#include <cstdlib>
#include <filesystem>

namespace elmo::core::conf {

Settings settings;

void Load() {
  if (const auto *APPNAME = std::getenv("APPNAME"); APPNAME && *APPNAME) {
    settings.APPNAME = APPNAME;
  }

  if (const auto *DB_PATH = std::getenv("DB_PATH")) {
    settings.DB_PATH = DB_PATH;
  }

  if (const auto *OTSC_CLIENT = std::getenv("OTSC_CLIENT")) {
    settings.OTSC_CLIENT = OTSC_CLIENT;
  }

  if (const auto *OTSC_SECRET = std::getenv("OTSC_SECRET")) {
    settings.OTSC_SECRET = OTSC_SECRET;
  }

  if (const auto *OTSC_TENANT = std::getenv("OTSC_TENANT")) {
    settings.OTSC_TENANT = OTSC_TENANT;
  }

  if (const auto *OTSC_REDIRECT_URI = std::getenv("OTSC_REDIRECT_URI")) {
    settings.OTSC_REDIRECT_URI = OTSC_REDIRECT_URI;
  }

  if (const std::filesystem::path p(settings.DB_PATH); p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path());
  }
}

} // namespace elmo::core::conf
