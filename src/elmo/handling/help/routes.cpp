#include "routes.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string_view>

#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>
#include <json/json.h>

#include "elmo/core/db.hpp"
#include "elmo/handling/auth/utils.hpp"
#include "elmo/handling/common.hpp"

namespace elmo::handling::help {

namespace common = elmo::handling::common;
namespace utils = elmo::handling::auth::utils;

namespace {

using common::Row;
using common::Rows;
using Db = std::unique_ptr<sqlite3, int (*)(sqlite3 *)>;

struct HelpEntry {
  std::string_view key;
  std::string_view section;
  std::string_view label;
};

constexpr std::string_view kHeader = "Encabezado";
constexpr std::string_view kParties = "Cliente / Emisor";
constexpr std::string_view kBilling = "Facturación / cobranza";
constexpr std::string_view kRent = "Alquiler";
constexpr std::string_view kTerms = "Observaciones y condiciones";
constexpr std::string_view kAdLines = "Espacio de publicidad (filas)";
constexpr std::string_view kTempLines = "Espacios temporales (filas)";
constexpr std::string_view kProductionLines = "Servicios de producción (filas)";

constexpr HelpEntry kCatalog[] = {
    {"order.number", kHeader, "N° pedido"},
    {"order.issued_on", kHeader, "F. emisión"},
    {"order.igv_percent", kHeader, "IGV %"},
    {"order.order_contract_kind_id", kHeader, "Tipo de contrato"},
    {"order.client_id", kParties, "Razón social del cliente"},
    {"order.brand", kParties, "Cliente / Marca"},
    {"order.contact_id", kParties, "Contacto del cliente"},
    {"order.issuer_id", kParties, "Empresa emisora"},
    {"order.issuer_contact_id", kParties, "Contacto del emisor"},
    {"party.ruc", kParties, "RUC"},
    {"party.address", kParties, "Dirección"},
    {"party.district", kParties, "Distrito"},
    {"order.billing_client_id", kBilling, "Razón social de facturación"},
    {"order.billing_contact_id", kBilling, "Contacto de pago"},
    {"order.payment_terms", kBilling, "Condición de pago"},
    {"rentorder.electric_power", kRent, "Energía eléctrica"},
    {"rentorder.agency_client_id", kRent, "Agencia y/o central de medios"},
    {"rentorder.guarantee", kRent, "¿Aplica garantía?"},
    {"rentorder.guarantee_amount", kRent, "Monto de garantía (S/)"},
    {"rentorder.guarantee_conditions", kRent, "Condiciones de garantía"},
    {"order.observations", kTerms, "Observaciones"},
    {"rentorder.terms", kTerms, "Términos y condiciones (alquiler)"},
    {"order.penalties", kTerms, "Penalidades"},
    {"order.other_conditions", kTerms, "Otras condiciones"},
    {"order.resolution", kTerms, "Resolución"},
    {"ad_space_line.code", kAdLines, "Código"},
    {"ad_space_line.element", kAdLines, "Elemento"},
    {"ad_space_line.location", kAdLines, "Ubicación / detalle"},
    {"ad_space_line.motive", kAdLines, "Motivo / marca"},
    {"ad_space_line.start_date", kAdLines, "Fecha inicio"},
    {"ad_space_line.end_date", kAdLines, "Fecha fin"},
    {"ad_space_line.element_qty", kAdLines, "Cant. elementos"},
    {"ad_space_line.periods", kAdLines, "Cant. periodos"},
    {"ad_space_line.period_unit", kAdLines, "Unidad"},
    {"ad_space_line.unit_price", kAdLines, "P. unitario (S/)"},
    {"ad_space_line.total", kAdLines, "P. total (S/)"},
    {"temp_space_line.code", kTempLines, "Código"},
    {"temp_space_line.location", kTempLines, "Ubicación / detalle"},
    {"temp_space_line.area", kTempLines, "Área"},
    {"temp_space_line.motive", kTempLines, "Motivo / marca"},
    {"temp_space_line.description", kTempLines, "Descripción"},
    {"temp_space_line.schedule", kTempLines, "Horario"},
    {"temp_space_line.start_date", kTempLines, "Fecha inicio"},
    {"temp_space_line.end_date", kTempLines, "Fecha fin"},
    {"temp_space_line.days", kTempLines, "Cant. días"},
    {"temp_space_line.total", kTempLines, "P. total (S/)"},
    {"production_line.code", kProductionLines, "Código"},
    {"production_line.location", kProductionLines, "Ubicación / detalle"},
    {"production_line.element", kProductionLines, "Elemento"},
    {"production_line.material", kProductionLines, "Material"},
    {"production_line.motive", kProductionLines, "Motivo / marca"},
    {"production_line.quantity", kProductionLines, "Cantidad"},
    {"production_line.width", kProductionLines, "Ancho"},
    {"production_line.height", kProductionLines, "Alto"},
    {"production_line.total", kProductionLines, "Total (S/)"},
};

constexpr std::string_view kTooLong =
    "El texto de ayuda admite hasta 500 caracteres.";
constexpr std::string_view kSaveFailed =
    "No se pudo guardar. Inténtalo de nuevo.";

const HelpEntry *FindEntry(std::string_view key) {
  const auto it = std::ranges::find(kCatalog, key, &HelpEntry::key);
  return it == std::end(kCatalog) ? nullptr : &*it;
}

struct DynamicKey {
  std::string table;
  std::string id;
};

std::optional<DynamicKey> ParseDynamic(std::string_view key) {
  for (const std::string_view table : {"form_block", "form_field"}) {
    if (key.size() > table.size() + 1 && key.starts_with(table) &&
        key[table.size()] == '.') {
      const std::string id(key.substr(table.size() + 1));
      if (common::IsId(id))
        return DynamicKey{std::string(table), id};
    }
  }
  return std::nullopt;
}

std::string Trim(std::string value) {
  const auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
              value.end());
  return value;
}

bool TooLong(const std::string &text) {
  return common::Utf8Length(text) > common::kHelpTextMax;
}

common::ExecResult UpsertFixed(sqlite3 *db, const std::string &key,
                               const std::string &text,
                               const std::string &username) {
  return common::Exec(
      db,
      "INSERT INTO order_field_help (key, help_text, updated_by) "
      "VALUES (?, ?, (SELECT id FROM auth_user WHERE username = ?)) "
      "ON CONFLICT (key) DO UPDATE SET help_text = excluded.help_text, "
      "updated_by = excluded.updated_by, updated_at = unixepoch()",
      {key, text, username});
}

void Reply(const Help::Callback &callback, drogon::HttpStatusCode status,
           std::string_view body) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(status);
  resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
  resp->setBody(std::string(body));
  callback(resp);
}

void Render(const Help::Callback &callback, const std::string &view,
            const drogon::HttpViewData &data,
            drogon::HttpStatusCode status = drogon::k200OK) {
  auto resp = drogon::HttpResponse::newHttpViewResponse(view, data);
  resp->setStatusCode(status);
  callback(resp);
}

void NotFound(const Help::Callback &callback) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k404NotFound);
  resp->setContentTypeCode(drogon::CT_TEXT_HTML);
  resp->setBody("<div role=\"alert\" class=\"alert alert-warning\">"
                "El campo no existe o fue eliminado.</div>");
  callback(resp);
}

Rows FetchEntries(sqlite3 *db) {
  Rows entries;
  Row saved;
  for (auto &row :
       common::FetchRows(db, "SELECT key, help_text FROM order_field_help"))
    saved[row["key"]] = row["help_text"];
  for (const auto &e : kCatalog) {
    const std::string key(e.key);
    const auto it = saved.find(key);
    entries.push_back({{"key", key},
                       {"section", std::string(e.section)},
                       {"label", std::string(e.label)},
                       {"help_text", it == saved.end() ? "" : it->second},
                       {"active", "1"}});
  }
  const auto blocks = common::FetchRows(
      db, "SELECT id, title, help_text, active FROM form_block "
          "ORDER BY title, id");
  const auto fields = common::FetchRows(
      db, "SELECT id, block_id, label, help_text, active FROM form_field "
          "ORDER BY position, id");
  for (const auto &b : blocks) {
    const auto section = "Bloque: " + b.at("title");
    entries.push_back({{"key", "form_block." + b.at("id")},
                       {"section", section},
                       {"label", "Título del bloque"},
                       {"help_text", b.at("help_text")},
                       {"active", b.at("active")}});
    for (const auto &f : fields) {
      if (f.at("block_id") != b.at("id"))
        continue;
      entries.push_back({{"key", "form_field." + f.at("id")},
                         {"section", section},
                         {"label", f.at("label")},
                         {"help_text", f.at("help_text")},
                         {"active", f.at("active")}});
    }
  }
  return entries;
}

std::optional<Row> FetchEntry(sqlite3 *db, const std::string &key) {
  if (const auto *e = FindEntry(key)) {
    const auto rows = common::FetchRows(
        db, "SELECT help_text FROM order_field_help WHERE key = ?", {key});
    return Row{{"key", key},
               {"section", std::string(e->section)},
               {"label", std::string(e->label)},
               {"help_text", rows.empty() ? "" : rows.front().at("help_text")}};
  }
  const auto dyn = ParseDynamic(key);
  if (!dyn)
    return std::nullopt;
  const auto rows = common::FetchRows(
      db,
      dyn->table == "form_block"
          ? "SELECT 'Bloque: ' || title AS section, 'Título del bloque' "
            "AS label, help_text FROM form_block WHERE id = ?"
          : "SELECT 'Bloque: ' || b.title AS section, f.label, f.help_text "
            "FROM form_field f JOIN form_block b ON b.id = f.block_id "
            "WHERE f.id = ?",
      {dyn->id});
  if (rows.empty())
    return std::nullopt;
  auto row = rows.front();
  row["key"] = key;
  return row;
}

void RenderList(const Help::Callback &callback, sqlite3 *db,
                const std::string &notice, const Row &updated) {
  drogon::HttpViewData data;
  data.insert("entries", FetchEntries(db));
  data.insert("notice", notice);
  data.insert("updated", updated);
  Render(callback, "help_list", data);
}

void RenderEdit(const Help::Callback &callback, const Row &entry,
                const std::string &error,
                drogon::HttpStatusCode status = drogon::k200OK) {
  drogon::HttpViewData data;
  data.insert("entry", entry);
  data.insert("error", error);
  Render(callback, "help_edit", data, status);
}

} // namespace

std::string FetchHelpJson() {
  Json::Value map(Json::objectValue);
  Db db{elmo::core::db::ConnectRO(), sqlite3_close_v2};
  if (db) {
    const auto rows = common::FetchRows(
        db.get(),
        "SELECT key, help_text FROM order_field_help WHERE help_text <> '' "
        "UNION ALL SELECT 'form_block.' || id, help_text FROM form_block "
        "WHERE help_text <> '' "
        "UNION ALL SELECT 'form_field.' || id, help_text FROM form_field "
        "WHERE help_text <> ''");
    for (const auto &row : rows)
      map[row.at("key")] = row.at("help_text");
  }
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  builder["emitUTF8"] = true;
  return Json::writeString(builder, map);
}

void Help::Save(const drogon::HttpRequestPtr &req, Callback &&callback,
                const std::string &key) {
  if (FindEntry(key) == nullptr)
    return Reply(callback, drogon::k422UnprocessableEntity, "Campo no válido.");
  const auto text = Trim(req->getParameter("help_text"));
  if (TooLong(text))
    return Reply(callback, drogon::k422UnprocessableEntity, kTooLong);
  Db db{elmo::core::db::Connect(), sqlite3_close_v2};
  if (!db)
    return common::ServiceUnavailable(callback);
  const auto res = UpsertFixed(
      db.get(), key, text, utils::SessionGet(req->session(), utils::kUsername));
  if (!res.ok())
    return Reply(callback, drogon::k503ServiceUnavailable, kSaveFailed);
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k204NoContent);
  callback(resp);
}

void Help::List(const drogon::HttpRequestPtr &, Callback &&callback) {
  Db db{elmo::core::db::ConnectRO(), sqlite3_close_v2};
  if (!db)
    return common::ServiceUnavailable(callback);
  RenderList(callback, db.get(), "", {});
}

void Help::EditGet(const drogon::HttpRequestPtr &req, Callback &&callback) {
  Db db{elmo::core::db::ConnectRO(), sqlite3_close_v2};
  if (!db)
    return common::ServiceUnavailable(callback);
  const auto entry = FetchEntry(db.get(), req->getParameter("key"));
  if (!entry)
    return NotFound(callback);
  RenderEdit(callback, *entry, "");
}

void Help::EditPost(const drogon::HttpRequestPtr &req, Callback &&callback) {
  Db db{elmo::core::db::Connect(), sqlite3_close_v2};
  if (!db)
    return common::ServiceUnavailable(callback);
  const auto key = req->getParameter("key");
  auto entry = FetchEntry(db.get(), key);
  if (!entry)
    return NotFound(callback);
  const auto text = Trim(req->getParameter("help_text"));
  (*entry)["help_text"] = text;
  if (TooLong(text))
    return RenderEdit(callback, *entry, std::string(kTooLong),
                      drogon::k422UnprocessableEntity);

  common::ExecResult res;
  if (FindEntry(key) != nullptr) {
    res = UpsertFixed(db.get(), key, text,
                      utils::SessionGet(req->session(), utils::kUsername));
  } else {
    const auto dyn = ParseDynamic(key);
    res = common::Exec(db.get(),
                       dyn->table == "form_block"
                           ? "UPDATE form_block SET help_text = ? WHERE id = ?"
                           : "UPDATE form_field SET help_text = ? WHERE id = ?",
                       {text, std::stoll(dyn->id)});
    if (res.ok() && res.changes == 0)
      return NotFound(callback);
  }
  if (!res.ok())
    return RenderEdit(callback, *entry, std::string(kSaveFailed),
                      drogon::k503ServiceUnavailable);
  RenderList(callback, db.get(),
             "Ayuda de «" + entry->at("label") + "» guardada.",
             {{"key", key}, {"help_text", text}});
}

} // namespace elmo::handling::help
