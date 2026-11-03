#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <sqlite3.h>

namespace elmo::testing {

[[nodiscard]] std::string ReadSource(std::string_view relative);

class TempDb {
public:
  TempDb();
  ~TempDb();

  TempDb(const TempDb &) = delete;
  TempDb &operator=(const TempDb &) = delete;

  void Exec(const std::string &sql) const;
  void LoadFixture() const;
  [[nodiscard]] std::string Scalar(const std::string &sql) const;
  [[nodiscard]] const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
  std::string previous_;
};

} // namespace elmo::testing
