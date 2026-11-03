#pragma once

#include <functional>

#include <drogon/HttpController.h>

namespace elmo::handling::home {

class Home : public drogon::HttpController<Home> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Home::Index, "/elmo/home", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  void Index(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace elmo::handling::home
