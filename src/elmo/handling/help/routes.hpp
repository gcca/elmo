#pragma once

#include <functional>
#include <string>

#include <drogon/HttpController.h>

namespace elmo::handling::help {

class Help : public drogon::HttpController<Help> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Help::Save, "/elmo/help/{key}", drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Help::List, "/elmo/help/manage/list", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Help::EditGet, "/elmo/help/manage/edit", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Help::EditPost, "/elmo/help/manage/edit", drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

  void Save(const drogon::HttpRequestPtr &req, Callback &&callback,
            const std::string &key);
  void List(const drogon::HttpRequestPtr &req, Callback &&callback);
  void EditGet(const drogon::HttpRequestPtr &req, Callback &&callback);
  void EditPost(const drogon::HttpRequestPtr &req, Callback &&callback);
};

[[nodiscard]] std::string FetchHelpJson();

} // namespace elmo::handling::help
