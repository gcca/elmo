#include <filesystem>
#include <string>

#include <drogon/HttpResponse.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <json/json.h>

#include "elmo/core/conf.hpp"
#include "elmo/testing/db.hpp"
#include "routes.hpp"

namespace elmo::handling::help {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::SaveArg;

using Response = drogon::HttpResponsePtr;

class HelpRoutesTest : public ::testing::Test {
protected:
  void SetUp() override { temp_.LoadFixture(); }

  static drogon::HttpRequestPtr
  Request(std::initializer_list<std::pair<std::string, std::string>> params) {
    auto req = drogon::HttpRequest::newHttpRequest();
    for (const auto &[k, v] : params)
      req->setParameter(k, v);
    return req;
  }

  template <typename Invoke> Response Call(Invoke invoke) {
    MockFunction<void(const Response &)> callback;
    Response response;
    EXPECT_CALL(callback, Call(_)).WillOnce(SaveArg<0>(&response));
    invoke(callback.AsStdFunction());
    return response;
  }

  static std::string Body(const Response &r) { return std::string(r->body()); }

  std::string Help(const std::string &key) const {
    return temp_.Scalar("SELECT help_text FROM order_field_help WHERE key = '" +
                        key + "'");
  }

  std::string FieldId(const std::string &code) const {
    return temp_.Scalar("SELECT id FROM form_field WHERE code = '" + code +
                        "'");
  }

  elmo::testing::TempDb temp_;
  help::Help controller_;
};

TEST_F(HelpRoutesTest, SaveUpsertsCatalogKey) {
  const auto r = Call([&](auto cb) {
    controller_.Save(Request({{"help_text", "  Marca comercial  "}}),
                     std::move(cb), "order.brand");
  });
  EXPECT_EQ(r->statusCode(), drogon::k204NoContent);
  EXPECT_EQ(Help("order.brand"), "Marca comercial");
  EXPECT_EQ(temp_.Scalar("SELECT quote(updated_by) FROM order_field_help "
                         "WHERE key = 'order.brand'"),
            "NULL");
}

TEST_F(HelpRoutesTest, SaveRejectsUnknownAndDynamicKeys) {
  for (const auto *key : {"order.nope", "form_field.1", "order", "x.y"}) {
    const auto r = Call([&](auto cb) {
      controller_.Save(Request({{"help_text", "x"}}), std::move(cb), key);
    });
    EXPECT_EQ(r->statusCode(), drogon::k422UnprocessableEntity) << key;
  }
  EXPECT_EQ(temp_.Scalar("SELECT count(*) FROM order_field_help WHERE key "
                         "NOT LIKE 'order.%' AND key NOT LIKE 'party.%' AND "
                         "key NOT LIKE 'rentorder.%' AND key NOT LIKE "
                         "'%_line.%'"),
            "0");
}

TEST_F(HelpRoutesTest, SaveLimitsLengthInCodePoints) {
  std::string ok;
  for (int i = 0; i < 500; ++i)
    ok += "ñ";
  const auto accepted = Call([&](auto cb) {
    controller_.Save(Request({{"help_text", ok}}), std::move(cb),
                     "order.payment_terms");
  });
  EXPECT_EQ(accepted->statusCode(), drogon::k204NoContent);
  const auto rejected = Call([&](auto cb) {
    controller_.Save(Request({{"help_text", ok + "ñ"}}), std::move(cb),
                     "order.payment_terms");
  });
  EXPECT_EQ(rejected->statusCode(), drogon::k422UnprocessableEntity);
  EXPECT_EQ(Help("order.payment_terms"), ok);
}

TEST_F(HelpRoutesTest, SaveReportsUnavailableDatabase) {
  const auto previous = core::conf::settings.DB_PATH;
  core::conf::settings.DB_PATH = "/nonexistent-dir/elmo.db";
  const auto r = Call([&](auto cb) {
    controller_.Save(Request({{"help_text", "x"}}), std::move(cb),
                     "order.brand");
  });
  core::conf::settings.DB_PATH = previous;
  EXPECT_EQ(r->statusCode(), drogon::k503ServiceUnavailable);
}

TEST_F(HelpRoutesTest, FetchHelpJsonIncludesFixedAndDynamicKeys) {
  Json::Value map;
  ASSERT_TRUE(Json::Reader().parse(FetchHelpJson(), map));
  EXPECT_EQ(map.size(), 55U + 6U + 26U);
  EXPECT_TRUE(map.isMember("order.client_id"));
  EXPECT_TRUE(map.isMember("form_field." + FieldId("budget")));
  temp_.Exec("UPDATE order_field_help SET help_text = '' WHERE key = "
             "'order.brand'");
  ASSERT_TRUE(Json::Reader().parse(FetchHelpJson(), map));
  EXPECT_FALSE(map.isMember("order.brand"));
}

TEST_F(HelpRoutesTest, FetchHelpJsonIsEmptyObjectWithoutDatabase) {
  const auto previous = core::conf::settings.DB_PATH;
  core::conf::settings.DB_PATH = "/nonexistent-dir/elmo.db";
  const auto json = FetchHelpJson();
  core::conf::settings.DB_PATH = previous;
  EXPECT_EQ(json, "{}");
}

TEST_F(HelpRoutesTest, ListRendersEveryEntryAndMissingHelp) {
  temp_.Exec("DELETE FROM order_field_help WHERE key = 'order.brand'");
  const auto r =
      Call([&](auto cb) { controller_.List(Request({}), std::move(cb)); });
  ASSERT_EQ(r->statusCode(), drogon::k200OK);
  const auto body = Body(r);
  EXPECT_THAT(body, HasSubstr("86 de 87 campos tienen ayuda"));
  EXPECT_THAT(body, HasSubstr("Sin ayuda"));
  EXPECT_THAT(body, HasSubstr("Bloque: Datos de campa"));
  EXPECT_THAT(body, HasSubstr("key=form_field." + FieldId("budget")));
  EXPECT_THAT(body, Not(HasSubstr("data-help-updated")));
}

TEST_F(HelpRoutesTest, EditGetFindsFixedAndDynamicKeys) {
  for (const auto &key :
       {std::string("order.brand"), "form_field." + FieldId("budget"),
        "form_block." + temp_.Scalar("SELECT id FROM form_block WHERE code = "
                                     "'campaign'")}) {
    const auto r = Call([&](auto cb) {
      controller_.EditGet(Request({{"key", key}}), std::move(cb));
    });
    EXPECT_EQ(r->statusCode(), drogon::k200OK) << key;
    EXPECT_THAT(Body(r), HasSubstr("name=\"key\" value=\"" + key + "\""));
  }
}

TEST_F(HelpRoutesTest, EditGetReturnsNotFoundForUnknownKeys) {
  for (const auto *key :
       {"", "order.nope", "form_field.99999", "form_field.1x", "form_x.1"}) {
    const auto r = Call([&](auto cb) {
      controller_.EditGet(Request({{"key", key}}), std::move(cb));
    });
    EXPECT_EQ(r->statusCode(), drogon::k404NotFound) << key;
  }
}

TEST_F(HelpRoutesTest, EditPostUpdatesDynamicFieldAndReturnsMarker) {
  const auto key = "form_field." + FieldId("budget");
  const auto r = Call([&](auto cb) {
    controller_.EditPost(Request({{"key", key}, {"help_text", "Soles <x>"}}),
                         std::move(cb));
  });
  ASSERT_EQ(r->statusCode(), drogon::k200OK);
  EXPECT_EQ(temp_.Scalar("SELECT help_text FROM form_field WHERE code = "
                         "'budget'"),
            "Soles <x>");
  EXPECT_THAT(Body(r), HasSubstr("data-key=\"" + key +
                                 "\" data-text=\"Soles &lt;x&gt;\""));
  EXPECT_THAT(Body(r), HasSubstr("alert-success"));
}

TEST_F(HelpRoutesTest, EditPostClearsFixedKeyAndRejectsOverLength) {
  const auto cleared = Call([&](auto cb) {
    controller_.EditPost(Request({{"key", "order.brand"}, {"help_text", ""}}),
                         std::move(cb));
  });
  EXPECT_EQ(cleared->statusCode(), drogon::k200OK);
  EXPECT_EQ(Help("order.brand"), "");

  const auto rejected = Call([&](auto cb) {
    controller_.EditPost(
        Request({{"key", "order.brand"}, {"help_text", std::string(501, 'a')}}),
        std::move(cb));
  });
  EXPECT_EQ(rejected->statusCode(), drogon::k422UnprocessableEntity);
  EXPECT_THAT(Body(rejected), HasSubstr("textarea-error"));
  EXPECT_EQ(Help("order.brand"), "");
}

TEST_F(HelpRoutesTest, EditPostReturnsNotFoundForMissingDynamicRow) {
  const auto r = Call([&](auto cb) {
    controller_.EditPost(
        Request({{"key", "form_block.99999"}, {"help_text", "x"}}),
        std::move(cb));
  });
  EXPECT_EQ(r->statusCode(), drogon::k404NotFound);
}

} // namespace
} // namespace elmo::handling::help
