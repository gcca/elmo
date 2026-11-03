#include "routes.hpp"

#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>

#include "elmo/core/db.hpp"
#include "elmo/handling/common.hpp"

namespace elmo::handling::party {

namespace common = elmo::handling::common;

void Party::ContactOptions(const drogon::HttpRequestPtr &req,
                           common::Callback &&callback) {
  const auto client_id = req->getParameter("client_id");
  const auto issuer_id = req->getParameter("issuer_id");

  common::Rows contacts;
  if (common::IsId(client_id) || common::IsId(issuer_id)) {
    sqlite3 *db = elmo::core::db::ConnectRO();
    if (!db)
      return common::ServiceUnavailable(callback);

    contacts = common::IsId(client_id)
                   ? common::FetchRows(db,
                                       "SELECT id, name, role, phone, email "
                                       "FROM party_contact WHERE client_id = ? "
                                       "ORDER BY name, id",
                                       {client_id})
                   : common::FetchRows(db,
                                       "SELECT id, name, role, phone, email "
                                       "FROM party_contact WHERE issuer_id = ? "
                                       "ORDER BY name, id",
                                       {issuer_id});
    sqlite3_close_v2(db);
  }

  drogon::HttpViewData data;
  data.insert("contacts", contacts);
  callback(
      drogon::HttpResponse::newHttpViewResponse("party_contact_options", data));
}

} // namespace elmo::handling::party
