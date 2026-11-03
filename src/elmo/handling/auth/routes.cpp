#include "routes.hpp"

#include <string>

#include <drogon/HttpClient.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>
#include <json/json.h>
#include <sqlite3.h>
#include <trantor/utils/Logger.h>

#include "elmo/core/conf.hpp"
#include "elmo/core/db.hpp"
#include "utils.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

namespace utils = elmo::handling::auth::utils;
using elmo::core::conf::settings;

constexpr const char *kMicrosoftHost = "https://login.microsoftonline.com";

[[nodiscard]] drogon::HttpResponsePtr
SignInView(std::string error, drogon::HttpStatusCode code = drogon::k200OK) {
  drogon::HttpViewData data;
  data.insert("appname", settings.APPNAME);
  data.insert("error", std::move(error));
  auto resp = drogon::HttpResponse::newHttpViewResponse("signin", data);
  resp->setStatusCode(code);
  return resp;
}

[[nodiscard]] drogon::HttpResponsePtr RedirectHome() {
  return drogon::HttpResponse::newRedirectionResponse(
      std::string(utils::kHomePath));
}

void StartSession(const drogon::SessionPtr &session, const utils::User &user) {
  utils::SessionSet(session, utils::kUsername, user.username);
  utils::SessionSet(session, utils::kName,
                    user.name.empty() ? user.username : user.name);
  session->changeSessionIdToClient();
}

[[nodiscard]] bool OtscConfigured() {
  return !settings.OTSC_CLIENT.empty() && !settings.OTSC_SECRET.empty() &&
         !settings.OTSC_TENANT.empty() && !settings.OTSC_REDIRECT_URI.empty();
}

} // namespace

namespace elmo::handling::auth {

void Auth::SignInGet(const drogon::HttpRequestPtr &req, Callback &&callback) {
  if (!utils::SessionGet(req->session(), utils::kUsername).empty())
    return callback(RedirectHome());
  callback(SignInView({}));
}

void Auth::SignInPost(const drogon::HttpRequestPtr &req, Callback &&callback) {
  const auto username = req->getParameter("username");
  const auto password = req->getParameter("password");

  if (username.empty() || password.empty())
    return callback(
        SignInView("Ingresa tu usuario y contraseña.", drogon::k400BadRequest));

  sqlite3 *db = core::db::Connect();
  if (!db)
    return callback(SignInView("Base de datos no disponible.",
                               drogon::k503ServiceUnavailable));

  const auto user = utils::Authenticate(db, username, password);
  if (user)
    utils::TouchLastLoggedIn(db, user->id);
  sqlite3_close(db);

  if (!user) {
    LOG_INFO << "[auth] password sign-in failed";
    return callback(SignInView("Usuario o contraseña incorrectos.",
                               drogon::k401Unauthorized));
  }

  StartSession(req->session(), *user);
  callback(RedirectHome());
}

void Auth::SignOutPost(const drogon::HttpRequestPtr &req, Callback &&callback) {
  if (const auto session = req->session()) {
    session->clear();
    session->changeSessionIdToClient();
  }
  callback(drogon::HttpResponse::newRedirectionResponse(
      std::string(utils::kSignInPath)));
}

void Auth::OtscGet(const drogon::HttpRequestPtr &req, Callback &&callback) {
  if (!OtscConfigured())
    return callback(SignInView("El inicio de sesión con Microsoft no está "
                               "configurado.",
                               drogon::k503ServiceUnavailable));

  const auto state = utils::RandomHex(32);
  if (state.empty())
    return callback(SignInView("No se pudo iniciar sesión con Microsoft.",
                               drogon::k500InternalServerError));
  utils::SessionSet(req->session(), utils::kOAuthState, state);

  const std::string url =
      std::string(kMicrosoftHost) + "/" +
      utils::UrlEncode(settings.OTSC_TENANT) +
      "/oauth2/v2.0/authorize?client_id=" +
      utils::UrlEncode(settings.OTSC_CLIENT) +
      "&response_type=code&response_mode=query" +
      "&redirect_uri=" + utils::UrlEncode(settings.OTSC_REDIRECT_URI) +
      "&scope=" + utils::UrlEncode("openid profile email") + "&state=" + state;

  callback(drogon::HttpResponse::newRedirectionResponse(url));
}

void Auth::OtscCallbackGet(const drogon::HttpRequestPtr &req,
                           Callback &&callback) {
  const auto session = req->session();
  const auto expected_state = utils::SessionGet(session, utils::kOAuthState);
  if (session)
    session->erase(std::string(utils::kOAuthState));

  const auto state = req->getParameter("state");
  if (expected_state.empty() || state != expected_state) {
    LOG_INFO << "[auth/otsc] state mismatch";
    return callback(SignInView("La sesión de inicio expiró. Intenta de nuevo.",
                               drogon::k400BadRequest));
  }

  if (const auto error = req->getParameter("error"); !error.empty()) {
    LOG_INFO << "[auth/otsc] authorize error: " << error;
    return callback(SignInView("Microsoft rechazó el inicio de sesión.",
                               drogon::k401Unauthorized));
  }

  const auto code = req->getParameter("code");
  if (code.empty())
    return callback(
        SignInView("Respuesta inválida de Microsoft.", drogon::k400BadRequest));

  if (!OtscConfigured())
    return callback(SignInView("El inicio de sesión con Microsoft no está "
                               "configurado.",
                               drogon::k503ServiceUnavailable));

  auto ms_req = drogon::HttpRequest::newHttpRequest();
  ms_req->setMethod(drogon::Post);
  ms_req->setPath("/" + utils::UrlEncode(settings.OTSC_TENANT) +
                  "/oauth2/v2.0/token");
  ms_req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);
  ms_req->setBody("grant_type=authorization_code&client_id=" +
                  utils::UrlEncode(settings.OTSC_CLIENT) +
                  "&client_secret=" + utils::UrlEncode(settings.OTSC_SECRET) +
                  "&code=" + utils::UrlEncode(code) + "&redirect_uri=" +
                  utils::UrlEncode(settings.OTSC_REDIRECT_URI) +
                  "&scope=" + utils::UrlEncode("openid profile email"));

  auto client = drogon::HttpClient::newHttpClient(kMicrosoftHost);
  client->sendRequest(
      ms_req,
      [client, session, cb = std::move(callback)](
          drogon::ReqResult result, const drogon::HttpResponsePtr &ms_resp) {
        if (result != drogon::ReqResult::Ok || !ms_resp) {
          LOG_ERROR << "[auth/otsc] token request failed: " << result;
          return cb(SignInView("No se pudo contactar a Microsoft.",
                               drogon::k502BadGateway));
        }

        const auto json = ms_resp->getJsonObject();
        if (!json) {
          LOG_ERROR << "[auth/otsc] token response is not JSON, status="
                    << ms_resp->statusCode();
          return cb(SignInView("Respuesta inválida de Microsoft.",
                               drogon::k502BadGateway));
        }

        if (json->isMember("error")) {
          LOG_INFO << "[auth/otsc] token error: "
                   << (*json)["error"].asString();
          return cb(SignInView("Microsoft rechazó el inicio de sesión.",
                               drogon::k401Unauthorized));
        }

        const auto claims =
            utils::DecodeJwtPayload((*json).get("id_token", "").asString());
        std::string username = claims.get("preferred_username", "").asString();
        if (username.empty())
          username = claims.get("upn", "").asString();
        if (username.empty()) {
          LOG_ERROR << "[auth/otsc] id_token without username claim";
          return cb(SignInView("No se pudo determinar tu identidad.",
                               drogon::k502BadGateway));
        }
        const std::string name = claims.get("name", "").asString();
        const std::string email = claims.get("email", username).asString();

        sqlite3 *db = core::db::Connect();
        if (!db)
          return cb(SignInView("Base de datos no disponible.",
                               drogon::k503ServiceUnavailable));
        const auto user = utils::UpsertOAuthUser(db, username, name, email);
        sqlite3_close(db);

        if (!user)
          return cb(SignInView("No se pudo registrar tu usuario.",
                               drogon::k500InternalServerError));

        StartSession(session, *user);
        LOG_INFO << "[auth/otsc] signed in user_id=" << user->id;
        cb(RedirectHome());
      },
      10.0);
}

} // namespace elmo::handling::auth
