#pragma once

#include <drogon/HttpMiddleware.h>

namespace elmo::handling::auth::middlewares {

class LogInRequired : public drogon::HttpMiddleware<LogInRequired> {
public:
  void invoke(const drogon::HttpRequestPtr &req,
              drogon::MiddlewareNextCallback &&nextCb,
              drogon::MiddlewareCallback &&mcb) override;
};

} // namespace elmo::handling::auth::middlewares
