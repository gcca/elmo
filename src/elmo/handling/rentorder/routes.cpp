#include "routes.hpp"

#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>

#include "elmo/core/db.hpp"
#include "elmo/handling/common.hpp"

namespace elmo::handling::rentorder {

namespace common = elmo::handling::common;

void RentOrder::Create(const drogon::HttpRequestPtr &,
                       common::Callback &&callback) {
  sqlite3 *db = elmo::core::db::ConnectRO();
  if (!db)
    return common::ServiceUnavailable(callback);

  auto contract_kinds = common::FetchRows(
      db, "SELECT id, code, name FROM rentorder_contract_kind ORDER BY id");
  auto clients = common::FetchClients(db);
  auto issuers = common::FetchIssuers(db);
  auto form = common::FetchFormDefinition(db, "rent");
  sqlite3_close_v2(db);

  const auto today = common::Today();

  drogon::HttpViewData data;
  data.insert("contract_kinds", contract_kinds);
  data.insert("clients", clients);
  data.insert("issuers", issuers);
  data.insert("blocks", form.blocks);
  data.insert("fields", form.fields);
  data.insert("options", form.options);
  data.insert("today", today);
  data.insert("year", today.substr(0, 4));
  callback(drogon::HttpResponse::newHttpViewResponse("rentorder_create", data));
}

} // namespace elmo::handling::rentorder
