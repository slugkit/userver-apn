#include "token.hpp"

#include <userver/crypto/base64.hpp>
#include <userver/crypto/signers.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/utils/datetime.hpp>

namespace apn::jwt {

namespace {

auto Base64UrlEncode(std::string_view data) -> std::string {
    return userver::crypto::base64::Base64UrlEncode(data, userver::crypto::base64::Pad::kWithout);
}

}  // namespace

auto GenerateToken(
    std::string_view key_pem,
    std::string_view key_id,
    std::string_view team_id
) -> std::string {
    namespace json = userver::formats::json;

    json::ValueBuilder header_builder;
    header_builder["alg"] = "ES256";
    header_builder["kid"] = key_id;
    auto header = json::ToString(header_builder.ExtractValue());

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                   userver::utils::datetime::Now().time_since_epoch()
    )
                   .count();

    json::ValueBuilder payload_builder;
    payload_builder["iss"] = team_id;
    payload_builder["iat"] = now;
    auto payload = json::ToString(payload_builder.ExtractValue());

    auto encoded_header = Base64UrlEncode(header);
    auto encoded_payload = Base64UrlEncode(payload);
    auto signing_input = fmt::format("{}.{}", encoded_header, encoded_payload);

    userver::crypto::SignerEs256 signer{std::string{key_pem}};
    auto signature_raw = signer.Sign({signing_input});
    auto encoded_signature = Base64UrlEncode(signature_raw);

    return fmt::format("{}.{}", signing_input, encoded_signature);
}

}  // namespace apn::jwt
