#pragma once

#include <functional>
#include <string>

#include <drogon/HttpController.h>

namespace elmo::handling::party {

class Party : public drogon::HttpController<Party> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Party::ContactOptions, "/elmo/party/contact/options",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Party::List, "/elmo/party/{entity}/list", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Party::CreateGet, "/elmo/party/{entity}/create", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Party::CreatePost, "/elmo/party/{entity}/create", drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Party::Details, "/elmo/party/{entity}/{id}/details",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Party::EditGet, "/elmo/party/{entity}/{id}/edit", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Party::EditPost, "/elmo/party/{entity}/{id}/edit", drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Party::DeletePost, "/elmo/party/{entity}/{id}/delete",
                drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

  void List(const drogon::HttpRequestPtr &req, Callback &&callback,
            const std::string &entity);
  void CreateGet(const drogon::HttpRequestPtr &req, Callback &&callback,
                 const std::string &entity);
  void CreatePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                  const std::string &entity);
  void Details(const drogon::HttpRequestPtr &req, Callback &&callback,
               const std::string &entity, const std::string &id);
  void EditGet(const drogon::HttpRequestPtr &req, Callback &&callback,
               const std::string &entity, const std::string &id);
  void EditPost(const drogon::HttpRequestPtr &req, Callback &&callback,
                const std::string &entity, const std::string &id);
  void DeletePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                  const std::string &entity, const std::string &id);

  void ContactOptions(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace elmo::handling::party
