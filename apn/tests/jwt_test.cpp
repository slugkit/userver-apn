#include "../src/apn/jwt/token.hpp"

#include <userver/crypto/base64.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/utest/utest.hpp>

namespace {

// A test ES256 (P-256) private key in PKCS#8 format. Not used for anything real.
constexpr std::string_view kTestKeyPem = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg0SDXkyuVprFEPSE7
8N8EiGiRBZ7D0zPakBVeklUlsDyhRANCAARI0sBcbBmalQeSxlasnuBXgeHD1EOC
UYVLhKa2JEIaaT7WMrsBptha+2THo4mx4YEpwu3KXq2bgbelo8jDsH7W
-----END PRIVATE KEY-----
)";

constexpr std::string_view kTestKeyId = "TESTKEY123";
constexpr std::string_view kTestTeamId = "TEAM123456";

}  // namespace

UTEST(ApnJwt, TokenHasThreeParts) {
    auto token = apn::jwt::GenerateToken(kTestKeyPem, kTestKeyId, kTestTeamId);
    int dot_count = 0;
    for (auto c : token) {
        if (c == '.') ++dot_count;
    }
    EXPECT_EQ(dot_count, 2);
}

UTEST(ApnJwt, HeaderContainsAlgAndKid) {
    auto token = apn::jwt::GenerateToken(kTestKeyPem, kTestKeyId, kTestTeamId);
    auto first_dot = token.find('.');
    ASSERT_NE(first_dot, std::string::npos);

    auto header_b64 = token.substr(0, first_dot);
    auto header_json = userver::crypto::base64::Base64UrlDecode(header_b64);
    auto header = userver::formats::json::FromString(header_json);

    EXPECT_EQ(header["alg"].As<std::string>(), "ES256");
    EXPECT_EQ(header["kid"].As<std::string>(), kTestKeyId);
}

UTEST(ApnJwt, PayloadContainsIssAndIat) {
    auto token = apn::jwt::GenerateToken(kTestKeyPem, kTestKeyId, kTestTeamId);
    auto first_dot = token.find('.');
    auto second_dot = token.find('.', first_dot + 1);
    ASSERT_NE(second_dot, std::string::npos);

    auto payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    auto payload_json = userver::crypto::base64::Base64UrlDecode(payload_b64);
    auto payload = userver::formats::json::FromString(payload_json);

    EXPECT_EQ(payload["iss"].As<std::string>(), kTestTeamId);
    EXPECT_TRUE(payload.HasMember("iat"));
    EXPECT_GT(payload["iat"].As<std::int64_t>(), 0);
}
