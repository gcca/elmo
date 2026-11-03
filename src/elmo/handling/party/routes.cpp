#include "routes.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>

#include "elmo/core/db.hpp"
#include "elmo/handling/common.hpp"

namespace elmo::handling::party {

namespace common = elmo::handling::common;

namespace {

using common::Bind;
using common::Row;
using common::Rows;
using Db = std::unique_ptr<sqlite3, int (*)(sqlite3 *)>;

struct Entity {
  std::string type;
  std::string title;
  std::string singular;
  std::string table;
  std::string owner_column;

  bool contact() const { return type == "contact"; }
};

const Entity *FindEntity(const std::string &type) {
  static const Entity entities[] = {
      {"client", "Clientes", "cliente", "party_client", "client_id"},
      {"issuer", "Emisores", "emisor", "party_issuer", "issuer_id"},
      {"contact", "Contactos", "contacto", "party_contact", ""}};
  for (const auto &entity : entities)
    if (entity.type == type)
      return &entity;
  return nullptr;
}

std::string At(const Row &row, const std::string &key) {
  const auto it = row.find(key);
  return it == row.end() ? "" : it->second;
}

std::string Param(const drogon::HttpRequestPtr &req, const char *key) {
  auto value = req->getParameter(key);
  const auto nonspace = [](unsigned char c) { return std::isspace(c) == 0; };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), nonspace));
  value.erase(std::find_if(value.rbegin(), value.rend(), nonspace).base(),
              value.end());
  return value;
}

std::optional<Row> FetchOne(sqlite3 *db, const std::string &sql,
                            const std::vector<std::string> &binds = {}) {
  auto rows = common::FetchRows(db, sql, binds);
  if (rows.empty())
    return std::nullopt;
  return std::move(rows.front());
}

constexpr std::string_view kContactSql =
    "SELECT p.id, p.name, p.role, p.phone, p.email, "
    "CASE WHEN p.client_id IS NOT NULL THEN 'client' ELSE 'issuer' END "
    "AS owner_type, COALESCE(p.client_id, p.issuer_id) AS owner_id, "
    "CASE WHEN p.client_id IS NOT NULL THEN 'client:' || p.client_id "
    "ELSE 'issuer:' || p.issuer_id END AS owner, "
    "COALESCE(c.name, i.name) AS owner_name, "
    "COALESCE(c.ruc, i.ruc) AS owner_ruc "
    "FROM party_contact p "
    "LEFT JOIN party_client c ON c.id = p.client_id "
    "LEFT JOIN party_issuer i ON i.id = p.issuer_id ";

std::optional<Row> FetchRecord(sqlite3 *db, const Entity &entity,
                               const std::string &id) {
  return FetchOne(db,
                  entity.contact() ? std::string(kContactSql) + "WHERE p.id = ?"
                                   : "SELECT id, name, ruc, address, district "
                                     "FROM " +
                                         entity.table + " WHERE id = ?",
                  {id});
}

Row Usage(sqlite3 *db, const Entity &entity, const std::string &id) {
  std::string rent;
  std::string production;
  if (entity.contact()) {
    rent = production =
        "contact_id = ?1 OR issuer_contact_id = ?1 OR billing_contact_id = ?1";
  } else if (entity.type == "client") {
    production = "client_id = ?1 OR billing_client_id = ?1";
    rent = production + " OR agency_client_id = ?1";
  } else {
    rent = production = "issuer_id = ?1";
  }
  const auto contacts = entity.contact()
                            ? std::string("0")
                            : "(SELECT COUNT(*) FROM party_contact WHERE " +
                                  entity.owner_column + " = ?1)";
  return FetchOne(
             db,
             "SELECT " + contacts +
                 " AS contacts, "
                 "(SELECT COUNT(*) FROM rentorder_header WHERE " +
                 rent +
                 ") + (SELECT COUNT(*) FROM productionorder_header WHERE " +
                 production + ") AS orders",
             {id})
      .value_or(Row{});
}

bool InUse(const Row &usage) {
  return At(usage, "orders") != "0" || At(usage, "contacts") != "0";
}

std::string InUseMessage(const Row &usage) {
  std::string message = "No se puede eliminar: tiene registros vinculados.";
  if (At(usage, "contacts") != "0")
    message += " Contactos: " + At(usage, "contacts") + ".";
  if (At(usage, "orders") != "0")
    message += " Órdenes: " + At(usage, "orders") + ".";
  return message;
}

drogon::HttpViewData ViewData(const Entity &entity) {
  drogon::HttpViewData data;
  data.insert("entity", Row{{"type", entity.type},
                            {"title", entity.title},
                            {"singular", entity.singular},
                            {"module", "party" + entity.type}});
  return data;
}

void Render(const Party::Callback &callback, const std::string &view,
            const drogon::HttpViewData &data,
            drogon::HttpStatusCode status = drogon::k200OK) {
  auto response = drogon::HttpResponse::newHttpViewResponse(view, data);
  response->setStatusCode(status);
  callback(response);
}

void NotFound(const Party::Callback &callback) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k404NotFound);
  response->setContentTypeCode(drogon::CT_TEXT_HTML);
  response->setBody("<div role=\"alert\" class=\"alert alert-warning\">"
                    "El registro no existe o fue eliminado.</div>");
  callback(response);
}

void RenderList(const Party::Callback &callback, sqlite3 *db,
                const Entity &entity, const std::string &notice = "") {
  auto data = ViewData(entity);
  data.insert(
      "rows",
      common::FetchRows(
          db, entity.contact()
                  ? std::string(kContactSql) + "ORDER BY p.name, p.id"
                  : "SELECT p.id, p.name, p.ruc, p.address, p.district, "
                    "(SELECT COUNT(*) FROM party_contact c WHERE c." +
                        entity.owner_column +
                        " = p.id) AS contacts "
                        "FROM " +
                        entity.table + " p ORDER BY p.name, p.id"));
  data.insert("notice", notice);
  Render(callback,
         entity.contact() ? "party_contact_list" : "party_company_list", data);
}

void RenderDetails(const Party::Callback &callback, sqlite3 *db,
                   const Entity &entity, const Row &record,
                   const std::string &notice = "",
                   const std::string &error = "",
                   drogon::HttpStatusCode status = drogon::k200OK) {
  auto data = ViewData(entity);
  data.insert("record", record);
  data.insert("notice", notice);
  data.insert("error", error);
  if (!entity.contact())
    data.insert("contacts",
                common::FetchRows(db,
                                  "SELECT id, name, role, phone, email "
                                  "FROM party_contact WHERE " +
                                      entity.owner_column +
                                      " = ? ORDER BY name, id",
                                  {At(record, "id")}));
  Render(callback,
         entity.contact() ? "party_contact_details" : "party_company_details",
         data, status);
}

void RenderForm(const Party::Callback &callback, sqlite3 *db,
                const Entity &entity, const Row &record,
                const std::string &mode, const Row &errors = {},
                bool locked = false,
                drogon::HttpStatusCode status = drogon::k200OK) {
  auto data = ViewData(entity);
  data.insert("record", record);
  data.insert("errors", errors);
  data.insert("mode", mode);
  data.insert("locked", locked);
  if (entity.contact()) {
    data.insert(
        "owners",
        common::FetchRows(db, "SELECT 'client:' || id AS owner, name, ruc, "
                              "'client' AS type FROM party_client "
                              "UNION ALL SELECT 'issuer:' || id, name, ruc, "
                              "'issuer' FROM party_issuer "
                              "ORDER BY type, name, owner"));
  }
  Render(callback,
         entity.contact() ? "party_contact_edit" : "party_company_edit", data,
         status);
}

std::optional<std::pair<const Entity *, std::string>>
ParseOwner(const std::string &value) {
  const auto colon = value.find(':');
  if (colon == std::string::npos)
    return std::nullopt;
  const auto *entity = FindEntity(value.substr(0, colon));
  auto id = value.substr(colon + 1);
  if (!entity || entity->contact() || !common::IsId(id))
    return std::nullopt;
  return std::pair{entity, std::move(id)};
}

void Save(const drogon::HttpRequestPtr &req, const Party::Callback &callback,
          sqlite3 *db, const Entity &entity, const Row &existing = {}) {
  const auto id = At(existing, "id");
  const bool editing = !id.empty();
  const auto mode = editing ? "edit" : "create";
  Row record{{"id", id}, {"name", Param(req, "name")}};
  Row errors;
  if (At(record, "name").empty())
    errors["name"] = "Ingresa el nombre.";
  bool locked = false;
  std::string sql;
  std::vector<Bind> binds;
  if (entity.contact()) {
    for (const auto *field : {"owner", "role", "phone", "email"})
      record[field] = Param(req, field);
    locked = editing && At(Usage(db, entity, id), "orders") != "0";
    if (locked && At(record, "owner").empty())
      record["owner"] = At(existing, "owner");
    const auto owner = ParseOwner(At(record, "owner"));
    if (!owner || !FetchRecord(db, *owner->first, owner->second))
      errors["owner"] = "Selecciona un cliente o emisor existente.";
    if (locked && At(record, "owner") != At(existing, "owner")) {
      errors["owner"] = "No se puede cambiar la empresa: el contacto está "
                        "vinculado a órdenes.";
      record["owner"] = At(existing, "owner");
    }
    if (owner) {
      const auto owner_id =
          static_cast<std::int64_t>(std::stoll(owner->second));
      binds = {At(record, "name"),
               At(record, "role"),
               At(record, "phone"),
               At(record, "email"),
               owner->first->type == "client" ? Bind(owner_id) : Bind(nullptr),
               owner->first->type == "issuer" ? Bind(owner_id) : Bind(nullptr)};
    }
    sql = editing ? "UPDATE party_contact SET name = ?, role = ?, phone = ?, "
                    "email = ?, client_id = ?, issuer_id = ? WHERE id = ?"
                  : "INSERT INTO party_contact "
                    "(name, role, phone, email, client_id, issuer_id) "
                    "VALUES (?, ?, ?, ?, ?, ?)";
  } else {
    for (const auto *field : {"ruc", "address", "district"})
      record[field] = Param(req, field);
    const auto ruc = At(record, "ruc");
    if (ruc.size() != 11 || !std::ranges::all_of(ruc, [](unsigned char c) {
          return c >= '0' && c <= '9';
        }))
      errors["ruc"] = "El RUC debe tener 11 dígitos.";
    binds = {At(record, "name"), ruc, At(record, "address"),
             At(record, "district")};
    sql = editing ? "UPDATE " + entity.table +
                        " SET name = ?, ruc = ?, address = ?, district = ? "
                        "WHERE id = ?"
                  : "INSERT INTO " + entity.table +
                        " (name, ruc, address, district) VALUES (?, ?, ?, ?)";
  }
  if (!errors.empty())
    return RenderForm(callback, db, entity, record, mode, errors, locked,
                      drogon::k422UnprocessableEntity);
  if (editing)
    binds.emplace_back(static_cast<std::int64_t>(std::stoll(id)));
  const auto result = common::Exec(db, sql, binds);
  if (!result.ok()) {
    if (result.unique())
      errors["ruc"] = "Ya existe un registro con este RUC.";
    else if (result.foreign_key())
      errors["owner"] = "La empresa ya no está disponible o el contacto tiene "
                        "órdenes vinculadas.";
    else
      return common::ServiceUnavailable(callback);
    return RenderForm(callback, db, entity, record, mode, errors, locked,
                      drogon::k422UnprocessableEntity);
  }
  if (editing && result.changes == 0)
    return NotFound(callback);
  const auto saved = FetchRecord(
      db, entity, editing ? id : std::to_string(result.last_insert_id));
  if (!saved)
    return common::ServiceUnavailable(callback);
  RenderDetails(callback, db, entity, *saved,
                editing ? "Cambios guardados." : "Registro creado.");
}

}

void Party::List(const drogon::HttpRequestPtr &, Callback &&callback,
                 const std::string &type) {
  const auto *entity = FindEntity(type);
  if (!entity)
    return NotFound(callback);
  Db db(elmo::core::db::ConnectRO(), sqlite3_close_v2);
  if (!db)
    return common::ServiceUnavailable(callback);
  RenderList(callback, db.get(), *entity);
}

void Party::CreateGet(const drogon::HttpRequestPtr &req, Callback &&callback,
                      const std::string &type) {
  const auto *entity = FindEntity(type);
  if (!entity)
    return NotFound(callback);
  Db db(elmo::core::db::ConnectRO(), sqlite3_close_v2);
  if (!db)
    return common::ServiceUnavailable(callback);
  Row record;
  if (entity->contact())
    record["owner"] = Param(req, "owner");
  RenderForm(callback, db.get(), *entity, record, "create");
}

void Party::CreatePost(const drogon::HttpRequestPtr &req, Callback &&callback,
                       const std::string &type) {
  const auto *entity = FindEntity(type);
  if (!entity)
    return NotFound(callback);
  Db db(elmo::core::db::Connect(), sqlite3_close_v2);
  if (!db)
    return common::ServiceUnavailable(callback);
  Save(req, callback, db.get(), *entity);
}

void Party::Details(const drogon::HttpRequestPtr &, Callback &&callback,
                    const std::string &type, const std::string &id) {
  const auto *entity = FindEntity(type);
  if (!entity || !common::IsId(id))
    return NotFound(callback);
  Db db(elmo::core::db::ConnectRO(), sqlite3_close_v2);
  if (!db)
    return common::ServiceUnavailable(callback);
  const auto record = FetchRecord(db.get(), *entity, id);
  if (!record)
    return NotFound(callback);
  RenderDetails(callback, db.get(), *entity, *record);
}

void Party::EditGet(const drogon::HttpRequestPtr &, Callback &&callback,
                    const std::string &type, const std::string &id) {
  const auto *entity = FindEntity(type);
  if (!entity || !common::IsId(id))
    return NotFound(callback);
  Db db(elmo::core::db::ConnectRO(), sqlite3_close_v2);
  if (!db)
    return common::ServiceUnavailable(callback);
  const auto record = FetchRecord(db.get(), *entity, id);
  if (!record)
    return NotFound(callback);
  const bool locked =
      entity->contact() && At(Usage(db.get(), *entity, id), "orders") != "0";
  RenderForm(callback, db.get(), *entity, *record, "edit", {}, locked);
}

void Party::EditPost(const drogon::HttpRequestPtr &req, Callback &&callback,
                     const std::string &type, const std::string &id) {
  const auto *entity = FindEntity(type);
  if (!entity || !common::IsId(id))
    return NotFound(callback);
  Db db(elmo::core::db::Connect(), sqlite3_close_v2);
  if (!db)
    return common::ServiceUnavailable(callback);
  const auto record = FetchRecord(db.get(), *entity, id);
  if (!record)
    return NotFound(callback);
  Save(req, callback, db.get(), *entity, *record);
}

void Party::DeletePost(const drogon::HttpRequestPtr &, Callback &&callback,
                       const std::string &type, const std::string &id) {
  const auto *entity = FindEntity(type);
  if (!entity || !common::IsId(id))
    return NotFound(callback);
  Db db(elmo::core::db::Connect(), sqlite3_close_v2);
  if (!db)
    return common::ServiceUnavailable(callback);
  const auto record = FetchRecord(db.get(), *entity, id);
  if (!record)
    return NotFound(callback);
  auto usage = Usage(db.get(), *entity, id);
  if (InUse(usage))
    return RenderDetails(callback, db.get(), *entity, *record, "",
                         InUseMessage(usage), drogon::k409Conflict);
  const auto result =
      common::Exec(db.get(), "DELETE FROM " + entity->table + " WHERE id = ?",
                   {static_cast<std::int64_t>(std::stoll(id))});
  if (!result.ok()) {
    if (!result.foreign_key())
      return common::ServiceUnavailable(callback);
    return RenderDetails(callback, db.get(), *entity, *record, "",
                         InUseMessage(Usage(db.get(), *entity, id)),
                         drogon::k409Conflict);
  }
  if (result.changes == 0)
    return NotFound(callback);
  RenderList(callback, db.get(), *entity,
             "Registro «" + At(*record, "name") + "» eliminado.");
}

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
