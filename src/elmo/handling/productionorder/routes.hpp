#pragma once

#include <functional>

#include <drogon/HttpController.h>

namespace elmo::handling::productionorder {

class ProductionOrder : public drogon::HttpController<ProductionOrder> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(ProductionOrder::Create, "/elmo/productionorder/create",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  void Create(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace elmo::handling::productionorder
