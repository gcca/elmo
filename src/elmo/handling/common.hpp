#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <drogon/HttpResponse.h>
#include <sqlite3.h>

#include "elmo/core/db.hpp"

namespace elmo::handling::common {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
using elmo::core::db::Row;
using elmo::core::db::Rows;

struct FormDefinition {
  Rows blocks;
  Rows fields;
  Rows options;
};

[[nodiscard]] Rows FetchRows(sqlite3 *db, std::string_view sql,
                             const std::vector<std::string> &binds = {});

[[nodiscard]] Rows FetchClients(sqlite3 *db);

[[nodiscard]] Rows FetchIssuers(sqlite3 *db);

[[nodiscard]] FormDefinition FetchFormDefinition(sqlite3 *db,
                                                 std::string_view family);

[[nodiscard]] bool IsId(std::string_view value);

[[nodiscard]] std::string Today();

void ServiceUnavailable(const Callback &callback);

} // namespace elmo::handling::common
