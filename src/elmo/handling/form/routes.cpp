#include "routes.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>

#include "elmo/core/db.hpp"
#include "elmo/handling/common.hpp"

namespace elmo::handling::form {

namespace common = elmo::handling::common;
using common::Bind;
using common::Row;
using common::Rows;

namespace {

using Db = std::unique_ptr<sqlite3, int (*)(sqlite3 *)>;

Db OpenRO() { return {elmo::core::db::ConnectRO(), sqlite3_close_v2}; }

Db OpenRW() { return {elmo::core::db::Connect(), sqlite3_close_v2}; }

constexpr std::string_view kBlockSql =
    "SELECT id, code, title, active FROM form_block WHERE id = ?";

constexpr std::string_view kBlockListSql =
    "SELECT b.id, b.code, b.title, b.active, "
    "(SELECT COUNT(*) FROM form_field f WHERE f.block_id = b.id) "
    "AS field_count "
    "FROM form_block b ORDER BY b.title, b.id";

constexpr std::string_view kAssignmentsSql =
    "SELECT bf.block_id, bf.family, bf.position, bf.active, "
    "COALESCE(bf.rent_contract_kind_id, bf.production_contract_kind_id, '') "
    "AS contract_kind_id, COALESCE(rk.name, pk.name, '') AS kind_name, "
    "(SELECT COUNT(*) FROM rentorder_field_value v "
    "WHERE v.block_id = bf.block_id AND bf.family = 'rent') + "
    "(SELECT COUNT(*) FROM productionorder_field_value v "
    "WHERE v.block_id = bf.block_id AND bf.family = 'production') "
    "AS value_count "
    "FROM form_block_family bf "
    "LEFT JOIN rentorder_contract_kind rk "
    "ON rk.id = bf.rent_contract_kind_id "
    "LEFT JOIN productionorder_contract_kind pk "
    "ON pk.id = bf.production_contract_kind_id "
    "WHERE ? = '' OR bf.block_id = ? "
    "ORDER BY bf.block_id, bf.family = 'production'";

constexpr std::string_view kFieldColumnsSql =
    "SELECT f.id, f.block_id, f.code, f.label, f.type, f.required, "
    "f.position, f.active, "
    "(SELECT COUNT(*) FROM form_field_option o WHERE o.field_id = f.id) "
    "AS option_count, "
    "(SELECT COUNT(*) FROM rentorder_field_value v WHERE v.field_id = f.id) + "
    "(SELECT COUNT(*) FROM productionorder_field_value v "
    "WHERE v.field_id = f.id) AS value_count "
    "FROM form_field f ";

constexpr std::string_view kOptionsSql =
    "SELECT o.id, o.field_id, o.code, o.label, o.position, o.active, "
    "(SELECT COUNT(*) FROM rentorder_field_value v WHERE v.option_id = o.id) "
    "+ (SELECT COUNT(*) FROM productionorder_field_value v "
    "WHERE v.option_id = o.id) "
    "+ (SELECT COUNT(*) FROM rentorder_field_value_option v "
    "WHERE v.option_id = o.id) "
    "+ (SELECT COUNT(*) FROM productionorder_field_value_option v "
    "WHERE v.option_id = o.id) AS value_count "
    "FROM form_field_option o ";

constexpr std::string_view kInUseBlock =
    "No se puede eliminar: el bloque tiene valores guardados en órdenes. "
    "Desactívalo en lugar de eliminarlo.";
constexpr std::string_view kInUseField =
    "No se puede eliminar: el campo tiene valores guardados en órdenes. "
    "Desactívalo en lugar de eliminarlo.";
constexpr std::string_view kInUseOption =
    "No se puede eliminar: la opción está elegida en órdenes. "
    "Desactívala en lugar de eliminarla.";
constexpr std::string_view kSaveFailed =
    "No se pudo guardar. Inténtalo de nuevo.";

constexpr std::string_view kFamilies[] = {"rent", "production"};

const Row &TypeLabels() {
  static const Row labels{
      {"number", "Número"},       {"text", "Texto"},
      {"option", "Opción única"}, {"multi_option", "Opción múltiple"},
      {"date", "Fecha"},          {"boolean", "Sí / No"}};
  return labels;
}

std::string FamilyLabel(std::string_view family) {
  return family == "rent" ? "Alquiler" : "Producción";
}

std::string Trim(std::string value) {
  const auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
              value.end());
  return value;
}

std::string Param(const drogon::HttpRequestPtr &req, const std::string &key) {
  return Trim(req->getParameter(key));
}

std::string Flag(const drogon::HttpRequestPtr &req, const std::string &key) {
  return req->getParameter(key) == "1" ? "1" : "0";
}

std::optional<std::int64_t> ParseInt(std::string_view text) {
  if (text.empty())
    return 0;
  std::int64_t value = 0;
  const auto *end = text.data() + text.size();
  const auto [ptr, ec] = std::from_chars(text.data(), end, value);
  if (ec != std::errc{} || ptr != end || value < -99999 || value > 99999)
    return std::nullopt;
  return value;
}

std::int64_t ToInt(std::string_view text) { return ParseInt(text).value_or(0); }

const std::string &At(const Row &row, const std::string &key) {
  static const std::string empty;
  const auto it = row.find(key);
  return it == row.end() ? empty : it->second;
}

std::optional<Row> FetchOne(sqlite3 *db, std::string_view sql,
                            const std::vector<std::string> &binds) {
  auto rows = common::FetchRows(db, sql, binds);
  if (rows.empty())
    return std::nullopt;
  return std::move(rows.front());
}

std::optional<Row> FetchField(sqlite3 *db, const std::string &field_id) {
  return FetchOne(db, std::string(kFieldColumnsSql) + "WHERE f.id = ?",
                  {field_id});
}

void Render(const Form::Callback &callback, const std::string &view,
            const drogon::HttpViewData &data,
            drogon::HttpStatusCode status = drogon::k200OK) {
  auto resp = drogon::HttpResponse::newHttpViewResponse(view, data);
  resp->setStatusCode(status);
  callback(resp);
}

void NotFound(const Form::Callback &callback) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k404NotFound);
  resp->setContentTypeCode(drogon::CT_TEXT_HTML);
  resp->setBody("<div role=\"alert\" class=\"alert alert-warning\">"
                "El registro no existe o fue eliminado.</div>");
  callback(resp);
}

void RenderList(const Form::Callback &callback, sqlite3 *db,
                const std::string &notice) {
  drogon::HttpViewData data;
  data.insert("blocks", common::FetchRows(db, kBlockListSql));
  data.insert("assignments", common::FetchRows(db, kAssignmentsSql, {"", ""}));
  data.insert("notice", notice);
  Render(callback, "form_block_list", data);
}

bool RenderBlockDetails(const Form::Callback &callback, sqlite3 *db,
                        const std::string &block_id, const std::string &notice,
                        const std::string &error,
                        drogon::HttpStatusCode status = drogon::k200OK) {
  auto block = FetchOne(db, kBlockSql, {block_id});
  if (!block)
    return false;
  drogon::HttpViewData data;
  data.insert("block", *block);
  data.insert("assignments",
              common::FetchRows(db, kAssignmentsSql, {block_id, block_id}));
  data.insert("fields", common::FetchRows(db,
                                          std::string(kFieldColumnsSql) +
                                              "WHERE f.block_id = ? "
                                              "ORDER BY f.position, f.id",
                                          {block_id}));
  data.insert("type_labels", TypeLabels());
  data.insert("notice", notice);
  data.insert("error", error);
  Render(callback, "form_block_details", data, status);
  return true;
}

Row DefaultOptionForm(const Rows &options) {
  std::int64_t next = 1;
  for (const auto &o : options)
    next = std::max(next, ToInt(At(o, "position")) + 1);
  return {{"code", ""},
          {"label", ""},
          {"position", std::to_string(next)},
          {"active", "1"}};
}

bool RenderFieldDetails(const Form::Callback &callback, sqlite3 *db,
                        const std::string &field_id, const std::string &notice,
                        const std::string &error,
                        const std::optional<Row> &option_form = std::nullopt,
                        const Row &option_errors = {},
                        drogon::HttpStatusCode status = drogon::k200OK) {
  auto field = FetchField(db, field_id);
  if (!field)
    return false;
  auto block = FetchOne(db, kBlockSql, {At(*field, "block_id")});
  if (!block)
    return false;
  auto options =
      common::FetchRows(db,
                        std::string(kOptionsSql) + "WHERE o.field_id = ? "
                                                   "ORDER BY o.position, o.id",
                        {field_id});
  drogon::HttpViewData data;
  data.insert("block", *block);
  data.insert("field", *field);
  data.insert("option_form",
              option_form ? *option_form : DefaultOptionForm(options));
  data.insert("options", std::move(options));
  data.insert("option_errors", option_errors);
  data.insert("type_labels", TypeLabels());
  data.insert("notice", notice);
  data.insert("error", error);
  Render(callback, "form_field_details", data, status);
  return true;
}

Rows FetchKinds(sqlite3 *db, std::string_view family) {
  return common::FetchRows(db, family == "rent"
                                   ? "SELECT id, name FROM "
                                     "rentorder_contract_kind ORDER BY id"
                                   : "SELECT id, name FROM "
                                     "productionorder_contract_kind "
                                     "ORDER BY id");
}

Row BlockFormFromRequest(const drogon::HttpRequestPtr &req) {
  Row form{{"code", Param(req, "code")},
           {"title", Param(req, "title")},
           {"active", Flag(req, "active")}};
  for (const auto family : kFamilies) {
    const std::string f(family);
    form[f + "_enabled"] = Flag(req, f + "_enabled");
    form[f + "_contract_kind_id"] = Param(req, f + "_contract_kind_id");
    form[f + "_position"] = Param(req, f + "_position");
    form[f + "_active"] = Flag(req, f + "_active");
  }
  return form;
}

Row BlockFormFromDb(const Row &block, const Rows &assignments) {
  Row form{{"id", At(block, "id")},
           {"code", At(block, "code")},
           {"title", At(block, "title")},
           {"active", At(block, "active")}};
  for (const auto family : kFamilies) {
    const std::string f(family);
    form[f + "_enabled"] = "0";
    form[f + "_contract_kind_id"] = "";
    form[f + "_position"] = "0";
    form[f + "_active"] = "1";
    for (const auto &a : assignments) {
      if (At(a, "family") != f)
        continue;
      form[f + "_enabled"] = "1";
      form[f + "_contract_kind_id"] = At(a, "contract_kind_id");
      form[f + "_position"] = At(a, "position");
      form[f + "_active"] = At(a, "active");
    }
  }
  return form;
}

bool HasKind(const Rows &kinds, const std::string &id) {
  return std::ranges::any_of(kinds,
                             [&](const Row &k) { return At(k, "id") == id; });
}

Row ValidateBlock(const Row &form, const Rows &rent_kinds,
                  const Rows &production_kinds) {
  Row errors;
  if (!common::IsCode(At(form, "code")))
    errors["code"] = "Usa letras minúsculas, números o _ (máx. 64).";
  if (At(form, "title").empty() || At(form, "title").size() > 200)
    errors["title"] = "Ingresa un título (máx. 200 caracteres).";
  for (const auto family : kFamilies) {
    const std::string f(family);
    const auto &kind = At(form, f + "_contract_kind_id");
    if (!kind.empty() &&
        !HasKind(f == "rent" ? rent_kinds : production_kinds, kind))
      errors[f + "_contract_kind_id"] = "Tipo de contrato no válido.";
    if (!ParseInt(At(form, f + "_position")))
      errors[f + "_position"] = "Ingresa un número entero.";
  }
  return errors;
}

Bind OptionalId(const std::string &value) {
  if (value.empty())
    return nullptr;
  return ToInt(value);
}

std::optional<std::int64_t> SaveBlock(sqlite3 *db, const Row &form,
                                      std::int64_t block_id, Row &errors,
                                      std::string &error) {
  if (!common::Begin(db)) {
    error = kSaveFailed;
    return std::nullopt;
  }

  const auto fail = [&]() -> std::optional<std::int64_t> {
    common::Rollback(db);
    return std::nullopt;
  };

  common::ExecResult res;
  if (block_id == 0) {
    res = common::Exec(
        db, "INSERT INTO form_block (code, title, active) VALUES (?, ?, ?)",
        {At(form, "code"), At(form, "title"), ToInt(At(form, "active"))});
    block_id = res.last_insert_id;
  } else {
    res = common::Exec(db,
                       "UPDATE form_block SET code = ?, title = ?, active = ? "
                       "WHERE id = ?",
                       {At(form, "code"), At(form, "title"),
                        ToInt(At(form, "active")), block_id});
  }
  if (!res.ok()) {
    if (res.unique())
      errors["code"] = "Ya existe un bloque con este código.";
    else
      error = kSaveFailed;
    return fail();
  }

  for (const auto family : kFamilies) {
    const std::string f(family);
    if (At(form, f + "_enabled") == "1") {
      const auto &kind = At(form, f + "_contract_kind_id");
      res = common::Exec(
          db,
          "INSERT INTO form_block_family (block_id, family, "
          "rent_contract_kind_id, production_contract_kind_id, position, "
          "active) VALUES (?, ?, ?, ?, ?, ?) "
          "ON CONFLICT (block_id, family) DO UPDATE SET "
          "rent_contract_kind_id = excluded.rent_contract_kind_id, "
          "production_contract_kind_id = "
          "excluded.production_contract_kind_id, "
          "position = excluded.position, active = excluded.active",
          {block_id, f, f == "rent" ? OptionalId(kind) : Bind{nullptr},
           f == "production" ? OptionalId(kind) : Bind{nullptr},
           ToInt(At(form, f + "_position")), ToInt(At(form, f + "_active"))});
    } else {
      res = common::Exec(db,
                         "DELETE FROM form_block_family "
                         "WHERE block_id = ? AND family = ?",
                         {block_id, f});
    }
    if (!res.ok()) {
      if (res.foreign_key())
        errors[f + "_enabled"] =
            "Hay órdenes con valores de este bloque en " + FamilyLabel(f) +
            "; desactiva la asignación en lugar de quitarla.";
      else
        error = kSaveFailed;
      return fail();
    }
  }

  if (!common::Commit(db)) {
    error = kSaveFailed;
    return fail();
  }
  return block_id;
}

void RenderBlockForm(const Form::Callback &callback, sqlite3 *db,
                     const std::string &mode, const Row &form,
                     const Row &errors, const std::string &error,
                     drogon::HttpStatusCode status = drogon::k200OK) {
  drogon::HttpViewData data;
  data.insert("mode", mode);
  data.insert("block", form);
  data.insert("rent_kinds", FetchKinds(db, "rent"));
  data.insert("production_kinds", FetchKinds(db, "production"));
  data.insert("errors", errors);
  data.insert("error", error);
  Render(callback, "form_block_edit", data, status);
}

Row FieldFormFromRequest(const drogon::HttpRequestPtr &req) {
  return {
      {"code", Param(req, "code")},         {"label", Param(req, "label")},
      {"type", Param(req, "type")},         {"required", Flag(req, "required")},
      {"position", Param(req, "position")}, {"active", Flag(req, "active")}};
}

Row ValidateField(const Row &form) {
  Row errors;
  if (!common::IsCode(At(form, "code")))
    errors["code"] = "Usa letras minúsculas, números o _ (máx. 64).";
  if (At(form, "label").empty() || At(form, "label").size() > 200)
    errors["label"] = "Ingresa una etiqueta (máx. 200 caracteres).";
  if (!TypeLabels().contains(At(form, "type")))
    errors["type"] = "Elige un tipo.";
  if (!ParseInt(At(form, "position")))
    errors["position"] = "Ingresa un número entero.";
  return errors;
}

bool TypeLocked(const Row &field) {
  return ToInt(At(field, "option_count")) + ToInt(At(field, "value_count")) > 0;
}

void RenderFieldForm(const Form::Callback &callback, const std::string &mode,
                     const Row &block, const Row &form, bool type_locked,
                     const Row &errors, const std::string &error,
                     drogon::HttpStatusCode status = drogon::k200OK) {
  drogon::HttpViewData data;
  data.insert("mode", mode);
  data.insert("block", block);
  data.insert("field", form);
  data.insert("type_locked", type_locked);
  data.insert("type_labels", TypeLabels());
  data.insert("errors", errors);
  data.insert("error", error);
  Render(callback, "form_field_edit", data, status);
}

Row OptionFormFromRequest(const drogon::HttpRequestPtr &req) {
  return {{"code", Param(req, "code")},
          {"label", Param(req, "label")},
          {"position", Param(req, "position")},
          {"active", Flag(req, "active")}};
}

Row ValidateOption(const Row &form) {
  Row errors;
  if (!common::IsCode(At(form, "code")))
    errors["code"] = "Usa letras minúsculas, números o _ (máx. 64).";
  if (At(form, "label").empty() || At(form, "label").size() > 200)
    errors["label"] = "Ingresa una etiqueta (máx. 200 caracteres).";
  if (!ParseInt(At(form, "position")))
    errors["position"] = "Ingresa un número entero.";
  return errors;
}

bool IsOptionType(const std::string &type) {
  return type == "option" || type == "multi_option";
}

void RenderOptionForm(const Form::Callback &callback, const Row &block,
                      const Row &field, const Row &form, const Row &errors,
                      const std::string &error,
                      drogon::HttpStatusCode status = drogon::k200OK) {
  drogon::HttpViewData data;
  data.insert("block", block);
  data.insert("field", field);
  data.insert("option", form);
  data.insert("errors", errors);
  data.insert("error", error);
  Render(callback, "form_option_edit", data, status);
}

} // namespace

void Form::BlockList(const drogon::HttpRequestPtr &, Callback &&callback) {
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  RenderList(callback, db.get(), "");
}

void Form::BlockCreateGet(const drogon::HttpRequestPtr &, Callback &&callback) {
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  Row form{{"code", ""}, {"title", ""}, {"active", "1"}};
  for (const auto family : kFamilies) {
    const std::string f(family);
    form[f + "_enabled"] = "0";
    form[f + "_contract_kind_id"] = "";
    form[f + "_position"] = "0";
    form[f + "_active"] = "1";
  }
  RenderBlockForm(callback, db.get(), "create", form, {}, "");
}

void Form::BlockCreatePost(const drogon::HttpRequestPtr &req,
                           Callback &&callback) {
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto form = BlockFormFromRequest(req);
  auto errors = ValidateBlock(form, FetchKinds(db.get(), "rent"),
                              FetchKinds(db.get(), "production"));
  std::string error;
  std::optional<std::int64_t> id;
  if (errors.empty())
    id = SaveBlock(db.get(), form, 0, errors, error);
  if (!id)
    return RenderBlockForm(callback, db.get(), "create", form, errors, error,
                           drogon::k422UnprocessableEntity);
  RenderBlockDetails(callback, db.get(), std::to_string(*id), "Bloque creado.",
                     "");
}

void Form::BlockDetails(const drogon::HttpRequestPtr &, Callback &&callback,
                        const std::string &block_id) {
  if (!common::IsId(block_id))
    return NotFound(callback);
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  if (!RenderBlockDetails(callback, db.get(), block_id, "", ""))
    NotFound(callback);
}

void Form::BlockEditGet(const drogon::HttpRequestPtr &, Callback &&callback,
                        const std::string &block_id) {
  if (!common::IsId(block_id))
    return NotFound(callback);
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto block = FetchOne(db.get(), kBlockSql, {block_id});
  if (!block)
    return NotFound(callback);
  auto assignments =
      common::FetchRows(db.get(), kAssignmentsSql, {block_id, block_id});
  RenderBlockForm(callback, db.get(), "edit",
                  BlockFormFromDb(*block, assignments), {}, "");
}

void Form::BlockEditPost(const drogon::HttpRequestPtr &req, Callback &&callback,
                         const std::string &block_id) {
  if (!common::IsId(block_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  if (!FetchOne(db.get(), kBlockSql, {block_id}))
    return NotFound(callback);
  auto form = BlockFormFromRequest(req);
  form["id"] = block_id;
  auto errors = ValidateBlock(form, FetchKinds(db.get(), "rent"),
                              FetchKinds(db.get(), "production"));
  std::string error;
  std::optional<std::int64_t> id;
  if (errors.empty())
    id = SaveBlock(db.get(), form, ToInt(block_id), errors, error);
  if (!id)
    return RenderBlockForm(callback, db.get(), "edit", form, errors, error,
                           drogon::k422UnprocessableEntity);
  RenderBlockDetails(callback, db.get(), block_id, "Cambios guardados.", "");
}

void Form::BlockDeletePost(const drogon::HttpRequestPtr &, Callback &&callback,
                           const std::string &block_id) {
  if (!common::IsId(block_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto block = FetchOne(db.get(), kBlockSql, {block_id});
  if (!block)
    return NotFound(callback);
  const auto res = common::Exec(db.get(), "DELETE FROM form_block WHERE id = ?",
                                {ToInt(block_id)});
  if (!res.ok()) {
    RenderBlockDetails(
        callback, db.get(), block_id, "",
        std::string(res.foreign_key() ? kInUseBlock : kSaveFailed),
        drogon::k409Conflict);
    return;
  }
  RenderList(callback, db.get(),
             "Bloque «" + At(*block, "title") + "» eliminado.");
}

void Form::FieldCreateGet(const drogon::HttpRequestPtr &, Callback &&callback,
                          const std::string &block_id) {
  if (!common::IsId(block_id))
    return NotFound(callback);
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto block = FetchOne(db.get(), kBlockSql, {block_id});
  if (!block)
    return NotFound(callback);
  auto count =
      common::FetchRows(db.get(),
                        "SELECT COALESCE(MAX(position), 0) + 1 AS next "
                        "FROM form_field WHERE block_id = ?",
                        {block_id});
  const Row form{{"code", ""},
                 {"label", ""},
                 {"type", "text"},
                 {"required", "0"},
                 {"position", count.empty() ? "1" : At(count.front(), "next")},
                 {"active", "1"}};
  RenderFieldForm(callback, "create", *block, form, false, {}, "");
}

void Form::FieldCreatePost(const drogon::HttpRequestPtr &req,
                           Callback &&callback, const std::string &block_id) {
  if (!common::IsId(block_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto block = FetchOne(db.get(), kBlockSql, {block_id});
  if (!block)
    return NotFound(callback);
  auto form = FieldFormFromRequest(req);
  auto errors = ValidateField(form);
  std::string error;
  if (errors.empty()) {
    const auto res = common::Exec(
        db.get(),
        "INSERT INTO form_field (block_id, code, label, type, required, "
        "position, active) VALUES (?, ?, ?, ?, ?, ?, ?)",
        {ToInt(block_id), At(form, "code"), At(form, "label"), At(form, "type"),
         ToInt(At(form, "required")), ToInt(At(form, "position")),
         ToInt(At(form, "active"))});
    if (res.ok()) {
      RenderFieldDetails(callback, db.get(), std::to_string(res.last_insert_id),
                         "Campo creado.", "");
      return;
    }
    if (res.unique())
      errors["code"] = "Ya existe un campo con este código en el bloque.";
    else
      error = kSaveFailed;
  }
  RenderFieldForm(callback, "create", *block, form, false, errors, error,
                  drogon::k422UnprocessableEntity);
}

void Form::FieldDetails(const drogon::HttpRequestPtr &, Callback &&callback,
                        const std::string &field_id) {
  if (!common::IsId(field_id))
    return NotFound(callback);
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  if (!RenderFieldDetails(callback, db.get(), field_id, "", ""))
    NotFound(callback);
}

void Form::FieldEditGet(const drogon::HttpRequestPtr &, Callback &&callback,
                        const std::string &field_id) {
  if (!common::IsId(field_id))
    return NotFound(callback);
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto field = FetchField(db.get(), field_id);
  if (!field)
    return NotFound(callback);
  auto block = FetchOne(db.get(), kBlockSql, {At(*field, "block_id")});
  if (!block)
    return NotFound(callback);
  RenderFieldForm(callback, "edit", *block, *field, TypeLocked(*field), {}, "");
}

void Form::FieldEditPost(const drogon::HttpRequestPtr &req, Callback &&callback,
                         const std::string &field_id) {
  if (!common::IsId(field_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto field = FetchField(db.get(), field_id);
  if (!field)
    return NotFound(callback);
  auto block = FetchOne(db.get(), kBlockSql, {At(*field, "block_id")});
  if (!block)
    return NotFound(callback);
  const bool locked = TypeLocked(*field);
  auto form = FieldFormFromRequest(req);
  form["id"] = field_id;
  if (locked)
    form["type"] = At(*field, "type");
  auto errors = ValidateField(form);
  std::string error;
  if (errors.empty()) {
    const auto res = common::Exec(
        db.get(),
        "UPDATE form_field SET code = ?, label = ?, type = ?, required = ?, "
        "position = ?, active = ? WHERE id = ?",
        {At(form, "code"), At(form, "label"), At(form, "type"),
         ToInt(At(form, "required")), ToInt(At(form, "position")),
         ToInt(At(form, "active")), ToInt(field_id)});
    if (res.ok()) {
      RenderFieldDetails(callback, db.get(), field_id, "Cambios guardados.",
                         "");
      return;
    }
    if (res.unique())
      errors["code"] = "Ya existe un campo con este código en el bloque.";
    else
      error = kSaveFailed;
  }
  RenderFieldForm(callback, "edit", *block, form, locked, errors, error,
                  drogon::k422UnprocessableEntity);
}

void Form::FieldDeletePost(const drogon::HttpRequestPtr &, Callback &&callback,
                           const std::string &field_id) {
  if (!common::IsId(field_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto field = FetchField(db.get(), field_id);
  if (!field)
    return NotFound(callback);
  const auto res = common::Exec(db.get(), "DELETE FROM form_field WHERE id = ?",
                                {ToInt(field_id)});
  if (!res.ok()) {
    RenderFieldDetails(
        callback, db.get(), field_id, "",
        std::string(res.foreign_key() ? kInUseField : kSaveFailed),
        std::nullopt, {}, drogon::k409Conflict);
    return;
  }
  RenderBlockDetails(callback, db.get(), At(*field, "block_id"),
                     "Campo «" + At(*field, "label") + "» eliminado.", "");
}

void Form::OptionCreatePost(const drogon::HttpRequestPtr &req,
                            Callback &&callback, const std::string &field_id) {
  if (!common::IsId(field_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto field = FetchField(db.get(), field_id);
  if (!field)
    return NotFound(callback);
  auto form = OptionFormFromRequest(req);
  if (!IsOptionType(At(*field, "type"))) {
    RenderFieldDetails(callback, db.get(), field_id, "",
                       "Solo los campos de opción única o múltiple tienen "
                       "opciones.",
                       form, {}, drogon::k422UnprocessableEntity);
    return;
  }
  auto errors = ValidateOption(form);
  std::string error;
  if (errors.empty()) {
    const auto res = common::Exec(
        db.get(),
        "INSERT INTO form_field_option (field_id, field_type, code, label, "
        "position, active) VALUES (?, ?, ?, ?, ?, ?)",
        {ToInt(field_id), At(*field, "type"), At(form, "code"),
         At(form, "label"), ToInt(At(form, "position")),
         ToInt(At(form, "active"))});
    if (res.ok()) {
      RenderFieldDetails(callback, db.get(), field_id, "Opción agregada.", "");
      return;
    }
    if (res.unique())
      errors["code"] = "Ya existe una opción con este código.";
    else
      error = kSaveFailed;
  }
  RenderFieldDetails(callback, db.get(), field_id, "", error, form, errors,
                     drogon::k422UnprocessableEntity);
}

void Form::OptionEditGet(const drogon::HttpRequestPtr &, Callback &&callback,
                         const std::string &option_id) {
  if (!common::IsId(option_id))
    return NotFound(callback);
  auto db = OpenRO();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto option = FetchOne(db.get(), std::string(kOptionsSql) + "WHERE o.id = ?",
                         {option_id});
  if (!option)
    return NotFound(callback);
  auto field = FetchField(db.get(), At(*option, "field_id"));
  auto block = field ? FetchOne(db.get(), kBlockSql, {At(*field, "block_id")})
                     : std::nullopt;
  if (!field || !block)
    return NotFound(callback);
  RenderOptionForm(callback, *block, *field, *option, {}, "");
}

void Form::OptionEditPost(const drogon::HttpRequestPtr &req,
                          Callback &&callback, const std::string &option_id) {
  if (!common::IsId(option_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto option = FetchOne(db.get(), std::string(kOptionsSql) + "WHERE o.id = ?",
                         {option_id});
  if (!option)
    return NotFound(callback);
  auto field = FetchField(db.get(), At(*option, "field_id"));
  auto block = field ? FetchOne(db.get(), kBlockSql, {At(*field, "block_id")})
                     : std::nullopt;
  if (!field || !block)
    return NotFound(callback);
  auto form = OptionFormFromRequest(req);
  form["id"] = option_id;
  auto errors = ValidateOption(form);
  std::string error;
  if (errors.empty()) {
    const auto res = common::Exec(
        db.get(),
        "UPDATE form_field_option SET code = ?, label = ?, position = ?, "
        "active = ? WHERE id = ?",
        {At(form, "code"), At(form, "label"), ToInt(At(form, "position")),
         ToInt(At(form, "active")), ToInt(option_id)});
    if (res.ok()) {
      RenderFieldDetails(callback, db.get(), At(*field, "id"),
                         "Opción actualizada.", "");
      return;
    }
    if (res.unique())
      errors["code"] = "Ya existe una opción con este código.";
    else
      error = kSaveFailed;
  }
  RenderOptionForm(callback, *block, *field, form, errors, error,
                   drogon::k422UnprocessableEntity);
}

void Form::OptionDeletePost(const drogon::HttpRequestPtr &, Callback &&callback,
                            const std::string &option_id) {
  if (!common::IsId(option_id))
    return NotFound(callback);
  auto db = OpenRW();
  if (!db)
    return common::ServiceUnavailable(callback);
  auto option = FetchOne(db.get(), std::string(kOptionsSql) + "WHERE o.id = ?",
                         {option_id});
  if (!option)
    return NotFound(callback);
  const auto res =
      common::Exec(db.get(), "DELETE FROM form_field_option WHERE id = ?",
                   {ToInt(option_id)});
  RenderFieldDetails(
      callback, db.get(), At(*option, "field_id"),
      res.ok() ? "Opción «" + At(*option, "label") + "» eliminada." : "",
      res.ok() ? ""
               : std::string(res.foreign_key() ? kInUseOption : kSaveFailed),
      std::nullopt, {}, res.ok() ? drogon::k200OK : drogon::k409Conflict);
}

} // namespace elmo::handling::form
