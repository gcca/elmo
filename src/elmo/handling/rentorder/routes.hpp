#pragma once

#include <functional>

#include <drogon/HttpController.h>

namespace elmo::handling::rentorder {

class RentOrder : public drogon::HttpController<RentOrder> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(RentOrder::Create, "/elmo/rentorder/create", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  void Create(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace elmo::handling::rentorder
