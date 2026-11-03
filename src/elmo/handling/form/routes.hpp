#pragma once

#include <functional>
#include <string>

#include <drogon/HttpController.h>

namespace elmo::handling::form {

class Form : public drogon::HttpController<Form> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Form::BlockList, "/elmo/form/block/list", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::BlockCreateGet, "/elmo/form/block/create", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::BlockCreatePost, "/elmo/form/block/create", drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::BlockDetails, "/elmo/form/block/{block_id}/details",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::BlockEditGet, "/elmo/form/block/{block_id}/edit",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::BlockEditPost, "/elmo/form/block/{block_id}/edit",
                drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::BlockDeletePost, "/elmo/form/block/{block_id}/delete",
                drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::FieldCreateGet,
                "/elmo/form/block/{block_id}/field/create", drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::FieldCreatePost,
                "/elmo/form/block/{block_id}/field/create", drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::FieldDetails, "/elmo/form/field/{field_id}/details",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::FieldEditGet, "/elmo/form/field/{field_id}/edit",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::FieldEditPost, "/elmo/form/field/{field_id}/edit",
                drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::FieldDeletePost, "/elmo/form/field/{field_id}/delete",
                drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::OptionCreatePost,
                "/elmo/form/field/{field_id}/option/create", drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::OptionEditGet, "/elmo/form/option/{option_id}/edit",
                drogon::Get,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::OptionEditPost, "/elmo/form/option/{option_id}/edit",
                drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  ADD_METHOD_TO(Form::OptionDeletePost, "/elmo/form/option/{option_id}/delete",
                drogon::Post,
                "elmo::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

  void BlockList(const drogon::HttpRequestPtr &req, Callback &&callback);
  void BlockCreateGet(const drogon::HttpRequestPtr &req, Callback &&callback);
  void BlockCreatePost(const drogon::HttpRequestPtr &req, Callback &&callback);
  void BlockDetails(const drogon::HttpRequestPtr &req, Callback &&callback,
                    const std::string &block_id);
  void BlockEditGet(const drogon::HttpRequestPtr &req, Callback &&callback,
                    const std::string &block_id);
  void BlockEditPost(const drogon::HttpRequestPtr &req, Callback &&callback,
                     const std::string &block_id);
  void BlockDeletePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                       const std::string &block_id);
  void FieldCreateGet(const drogon::HttpRequestPtr &req, Callback &&callback,
                      const std::string &block_id);
  void FieldCreatePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                       const std::string &block_id);
  void FieldDetails(const drogon::HttpRequestPtr &req, Callback &&callback,
                    const std::string &field_id);
  void FieldEditGet(const drogon::HttpRequestPtr &req, Callback &&callback,
                    const std::string &field_id);
  void FieldEditPost(const drogon::HttpRequestPtr &req, Callback &&callback,
                     const std::string &field_id);
  void FieldDeletePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                       const std::string &field_id);
  void OptionCreatePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                        const std::string &field_id);
  void OptionEditGet(const drogon::HttpRequestPtr &req, Callback &&callback,
                     const std::string &option_id);
  void OptionEditPost(const drogon::HttpRequestPtr &req, Callback &&callback,
                      const std::string &option_id);
  void OptionDeletePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                        const std::string &option_id);
};

} // namespace elmo::handling::form
