#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

#include "elmo/core/conf.hpp"

int main() {
  using namespace std::chrono_literals;

  elmo::core::conf::Load();
  const auto &settings = elmo::core::conf::settings;

  constexpr auto kBind = "0.0.0.0";
  constexpr unsigned short kPort = 8000;

  LOG_INFO << "elmo: starting bind=" << kBind << " port=" << kPort
           << " db_path=" << settings.DB_PATH
           << " otsc_tenant=" << settings.OTSC_TENANT
           << " otsc_redirect_uri=" << settings.OTSC_REDIRECT_URI
           << " otsc_client="
           << (settings.OTSC_CLIENT.empty() ? "unset" : "set")
           << " otsc_secret="
           << (settings.OTSC_SECRET.empty() ? "unset" : "set");

  drogon::app()
      .enableSession(24h, drogon::Cookie::SameSite::kLax)
      .addListener(kBind, kPort)
      .run();

  std::fflush(nullptr);
  std::_Exit(EXIT_SUCCESS);
}
