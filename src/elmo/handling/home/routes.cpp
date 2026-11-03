#include "routes.hpp"

#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>

#include "elmo/core/conf.hpp"
#include "elmo/handling/auth/utils.hpp"
#include "elmo/handling/help/routes.hpp"

namespace elmo::handling::home {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

namespace utils = elmo::handling::auth::utils;

void Home::Index(const drogon::HttpRequestPtr &req, Callback &&callback) {
  const auto session = req->session();

  drogon::HttpViewData data;
  data.insert("appname", elmo::core::conf::settings.APPNAME);
  data.insert("username", utils::SessionGet(session, utils::kUsername));
  data.insert("name", utils::SessionGet(session, utils::kName));
  data.insert("help_map", elmo::handling::help::FetchHelpJson());
  callback(drogon::HttpResponse::newHttpViewResponse("home", data));
}

} // namespace elmo::handling::home
