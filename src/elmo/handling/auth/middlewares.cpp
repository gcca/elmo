#include "middlewares.hpp"

#include <string>

#include <drogon/HttpResponse.h>

#include "utils.hpp"

namespace elmo::handling::auth::middlewares {

void LogInRequired::invoke(const drogon::HttpRequestPtr &req,
                           drogon::MiddlewareNextCallback &&nextCb,
                           drogon::MiddlewareCallback &&mcb) {
  if (utils::SessionGet(req->session(), utils::kUsername).empty()) {
    return mcb(drogon::HttpResponse::newRedirectionResponse(
        std::string(utils::kSignInPath)));
  }
  nextCb(std::move(mcb));
}

} // namespace elmo::handling::auth::middlewares
