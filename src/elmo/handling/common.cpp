#include "common.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <format>

#include <trantor/utils/Logger.h>

namespace elmo::handling::common {

Rows FetchRows(sqlite3 *db, std::string_view sql,
               const std::vector<std::string> &binds) {
  Rows rows;
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt,
                         nullptr) != SQLITE_OK) {
    LOG_ERROR << "[common] prepare error: " << sqlite3_errmsg(db);
    return rows;
  }

  for (std::size_t i = 0; i < binds.size(); ++i)
    sqlite3_bind_text(stmt, static_cast<int>(i + 1), binds[i].c_str(), -1,
                      SQLITE_TRANSIENT);

  const int columns = sqlite3_column_count(stmt);
  int rc = SQLITE_ROW;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    Row row;
    for (int c = 0; c < columns; ++c)
      row.emplace(sqlite3_column_name(stmt, c),
                  elmo::core::db::ColumnText(stmt, c));
    rows.push_back(std::move(row));
  }
  if (rc != SQLITE_DONE)
    LOG_ERROR << "[common] step error: " << sqlite3_errmsg(db);

  sqlite3_finalize(stmt);
  return rows;
}

Rows FetchClients(sqlite3 *db) {
  return FetchRows(db, "SELECT id, name, ruc, address, district "
                       "FROM party_client ORDER BY name, id");
}

Rows FetchIssuers(sqlite3 *db) {
  return FetchRows(db, "SELECT id, name, ruc, address, district "
                       "FROM party_issuer ORDER BY name, id");
}

FormDefinition FetchFormDefinition(sqlite3 *db, std::string_view family) {
  const std::vector<std::string> binds{std::string(family)};
  FormDefinition def;
  def.blocks = FetchRows(
      db,
      "SELECT b.id, b.code, b.title, "
      "CASE bf.family WHEN 'rent' THEN bf.rent_contract_kind_id "
      "ELSE bf.production_contract_kind_id END AS contract_kind_id "
      "FROM form_block b JOIN form_block_family bf ON bf.block_id = b.id "
      "WHERE bf.family = ? AND b.active = 1 AND bf.active = 1 "
      "ORDER BY bf.position, b.id",
      binds);
  def.fields =
      FetchRows(db,
                "SELECT f.id, f.block_id, f.code, f.label, f.type, f.required "
                "FROM form_field f "
                "JOIN form_block b ON b.id = f.block_id "
                "JOIN form_block_family bf ON bf.block_id = f.block_id "
                "WHERE bf.family = ? AND f.active = 1 AND b.active = 1 "
                "AND bf.active = 1 ORDER BY f.position, f.id",
                binds);
  def.options =
      FetchRows(db,
                "SELECT o.id, o.field_id, o.code, o.label "
                "FROM form_field_option o "
                "JOIN form_field f ON f.id = o.field_id "
                "JOIN form_block_family bf ON bf.block_id = f.block_id "
                "WHERE bf.family = ? AND o.active = 1 AND f.active = 1 "
                "ORDER BY o.position, o.id",
                binds);
  return def;
}

bool IsId(std::string_view value) {
  return !value.empty() && value.size() <= 18 &&
         std::ranges::all_of(
             value, [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

std::string Today() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  return std::format("{:04}-{:02}-{:02}", local.tm_year + 1900,
                     local.tm_mon + 1, local.tm_mday);
}

void ServiceUnavailable(const Callback &callback) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k503ServiceUnavailable);
  resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
  resp->setBody("Base de datos no disponible");
  callback(resp);
}

} // namespace elmo::handling::common
