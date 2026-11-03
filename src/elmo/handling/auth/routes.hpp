#pragma once

#include <functional>

#include <drogon/HttpController.h>

namespace elmo::handling::auth {

class Auth : public drogon::HttpController<Auth> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Auth::SignInGet, "/elmo/auth/signin", drogon::Get);
  ADD_METHOD_TO(Auth::SignInPost, "/elmo/auth/signin", drogon::Post);
  ADD_METHOD_TO(Auth::SignOutPost, "/elmo/auth/signout", drogon::Post);
  ADD_METHOD_TO(Auth::OtscGet, "/elmo/auth/otsc", drogon::Get);
  ADD_METHOD_TO(Auth::OtscCallbackGet, "/elmo/auth/otsc/callback", drogon::Get);
  METHOD_LIST_END

  void
  SignInGet(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  SignInPost(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  SignOutPost(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void OtscGet(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void OtscCallbackGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace elmo::handling::auth
