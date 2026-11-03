#pragma once

#include <functional>

#include <drogon/HttpController.h>

namespace elmo::handling::index {

class Index : public drogon::HttpController<Index> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Index::Root, "/", drogon::Get);
  ADD_METHOD_TO(Index::Healthcheck, "/healthcheck", drogon::Get);
  METHOD_LIST_END

  void Root(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  Healthcheck(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace elmo::handling::index
