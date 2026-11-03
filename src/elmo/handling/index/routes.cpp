#include "routes.hpp"

namespace elmo::handling::index {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

void Index::Root(const drogon::HttpRequestPtr &, Callback &&callback) {
  callback(drogon::HttpResponse::newRedirectionResponse("/elmo/auth/signin"));
}

void Index::Healthcheck(const drogon::HttpRequestPtr &, Callback &&callback) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setBody("🍺");
  callback(response);
}

} // namespace elmo::handling::index
