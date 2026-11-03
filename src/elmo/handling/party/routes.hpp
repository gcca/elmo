#pragma once

#include <functional>

#include <drogon/HttpController.h>

namespace elmo::handling::party {

class Party : public drogon::HttpController<Party> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Party::ContactOptions, "/elmo/party/contact/options",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  void ContactOptions(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace elmo::handling::party
