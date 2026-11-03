#include <functional>
#include <string>

#include <drogon/HttpResponse.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "middlewares.hpp"

namespace elmo::handling::auth::middlewares {
namespace {

using ::testing::_;
using ::testing::MockFunction;
using ::testing::SaveArg;

using Response = drogon::HttpResponsePtr;
using Respond = std::function<void(const Response &)>;

TEST(LogInRequiredTest, RedirectsAnonymousRequestToSignIn) {
  MockFunction<void(Respond &&)> next;
  MockFunction<void(const Response &)> respond;
  Response response;
  EXPECT_CALL(next, Call(_)).Times(0);
  EXPECT_CALL(respond, Call(_)).WillOnce(SaveArg<0>(&response));

  LogInRequired middleware;
  middleware.invoke(drogon::HttpRequest::newHttpRequest(), next.AsStdFunction(),
                    respond.AsStdFunction());

  ASSERT_NE(response, nullptr);
  EXPECT_EQ(response->statusCode(), drogon::k302Found);
  EXPECT_EQ(response->getHeader("location"), "/elmo/auth/signin");
}

} // namespace
} // namespace elmo::handling::auth::middlewares
