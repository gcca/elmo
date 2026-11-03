#pragma once

#include <string>

namespace elmo::core::conf {

struct Settings {
  std::string APPNAME = "Elmo";
  std::string DB_PATH = "data/elmo.db";
  std::string OTSC_CLIENT;
  std::string OTSC_SECRET;
  std::string OTSC_TENANT = "organizations";
  std::string OTSC_REDIRECT_URI =
      "http://localhost:8000/elmo/auth/otsc/callback";
};

extern Settings settings;

void Load();

} // namespace elmo::core::conf
