#include <apn/components/client.hpp>

#include "../jwt/token.hpp"

#include <userver/clients/http/client.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/http/http_version.hpp>
#include <userver/crypto/signers.hpp>
#include <userver/http/predefined_header.hpp>
#include <userver/logging/log.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include <chrono>
#include <mutex>

namespace apn {

namespace {

constexpr std::string_view kProductionUrl = "https://api.push.apple.com";
constexpr std::string_view kSandboxUrl = "https://api.sandbox.push.apple.com";
constexpr auto kDefaultTokenRefreshInterval = std::chrono::minutes{50};
constexpr auto kDefaultRequestTimeout = std::chrono::seconds{10};

constexpr userver::http::headers::PredefinedHeader kApnsId{"apns-id"};

auto BaseUrl(bool use_sandbox) -> std::string_view {
    return use_sandbox ? kSandboxUrl : kProductionUrl;
}

auto DoSend(
    userver::clients::http::Client& http_client,
    std::string_view base_url,
    std::string_view bearer_token,
    const Notification& notification,
    std::string_view default_topic,
    std::chrono::milliseconds timeout
) -> SendResult {
    if (notification.device_token.empty()) {
        return SendResult{.status_code = 400, .apns_id = {}, .reason = "Empty device_token"};
    }

    auto url = fmt::format("{}/3/device/{}", base_url, notification.device_token);
    auto topic = notification.topic.empty() ? std::string{default_topic} : notification.topic;

    auto request = http_client.CreateRequest()
                       .post(url, notification.payload)
                       .http_version(userver::http::HttpVersion::k2Tls)
                       .headers({
                           {"authorization", fmt::format("bearer {}", bearer_token)},
                           {"apns-topic", topic},
                           {"apns-push-type", notification.push_type},
                           {"apns-priority", std::to_string(notification.priority)},
                       })
                       .timeout(timeout);

    if (notification.expiration != 0) {
        request.headers({{"apns-expiration", std::to_string(notification.expiration)}});
    }
    if (!notification.collapse_id.empty()) {
        request.headers({{"apns-collapse-id", notification.collapse_id}});
    }

    std::shared_ptr<userver::clients::http::Response> response;
    try {
        response = request.perform();
    } catch (const std::exception& e) {
        LOG_ERROR() << "APNs request failed: " << e.what();
        return SendResult{.status_code = 0, .apns_id = {}, .reason = e.what()};
    }

    auto status = static_cast<std::int32_t>(response->status_code());

    auto it = response->headers().find(kApnsId);
    auto apns_id = (it != response->headers().end()) ? std::string{it->second} : std::string{};

    std::string reason;
    if (status != 200) {
        auto body = response->body();
        LOG_WARNING() << "APNs error: status=" << status << ", body=" << body;
        try {
            auto json = userver::formats::json::FromString(body);
            reason = json["reason"].As<std::string>("");
        } catch (const std::exception&) {
            reason = body;
        }
    }

    return SendResult{
        .status_code = status,
        .apns_id = std::move(apns_id),
        .reason = std::move(reason),
    };
}

}  // namespace

struct Client::Impl {
    userver::components::HttpClient& http_client;
    Credentials credentials;
    std::string base_url;
    std::chrono::milliseconds request_timeout;
    std::chrono::seconds token_refresh_interval;

    mutable std::mutex token_mutex;
    mutable std::string token;
    mutable std::chrono::system_clock::time_point token_issued_at;

    Impl(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    )
        : http_client(context.FindComponent<userver::components::HttpClient>())
        , credentials(Credentials::FromConfig(config))
        , base_url(BaseUrl(credentials.use_sandbox))
        , request_timeout(config["request-timeout"].As<std::chrono::milliseconds>(kDefaultRequestTimeout))
        , token_refresh_interval(config["token-refresh-interval"].As<std::chrono::seconds>(
              kDefaultTokenRefreshInterval
          )) {
        if (credentials.key_pem.empty()) {
            throw std::runtime_error("apn-client: key-pem is not configured (set APN_KEY_PEM)");
        }
        if (credentials.key_id.empty()) {
            throw std::runtime_error("apn-client: key-id is not configured (set APN_KEY_ID)");
        }
        if (credentials.team_id.empty()) {
            throw std::runtime_error("apn-client: team-id is not configured (set APN_TEAM_ID)");
        }
        // Validate that the PEM can be parsed into a signer
        try {
            userver::crypto::SignerEs256{credentials.key_pem};
        } catch (const std::exception& e) {
            throw std::runtime_error(fmt::format("apn-client: invalid key-pem: {}", e.what()));
        }
    }

    auto EnsureToken() const -> std::string {
        std::lock_guard lock{token_mutex};
        auto now = userver::utils::datetime::Now();
        if (token.empty() || (now - token_issued_at) >= token_refresh_interval) {
            token = jwt::GenerateToken(credentials.key_pem, credentials.key_id, credentials.team_id);
            token_issued_at = now;
            LOG_INFO() << "APNs JWT refreshed";
        }
        return token;
    }

    auto Send(const Notification& notification) const -> SendResult {
        auto bearer = EnsureToken();
        return DoSend(
            http_client.GetHttpClient(),
            base_url,
            bearer,
            notification,
            credentials.topic,
            request_timeout
        );
    }

    auto Send(const Credentials& creds, const Notification& notification) const -> SendResult {
        auto bearer = jwt::GenerateToken(creds.key_pem, creds.key_id, creds.team_id);
        return DoSend(
            http_client.GetHttpClient(),
            BaseUrl(creds.use_sandbox),
            bearer,
            notification,
            creds.topic,
            request_timeout
        );
    }
};

Client::Client(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::components::ComponentBase(config, context)
    , impl_{config, context} {}

Client::~Client() = default;

auto Client::GetStaticConfigSchema() -> userver::yaml_config::Schema {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Apple Push Notification service client
additionalProperties: false
properties:
    key-pem:
        type: string
        description: PEM content of the .p8 auth key
    key-id:
        type: string
        description: 10-character Key ID from Apple Developer
    team-id:
        type: string
        description: 10-character Team ID from Apple Developer
    topic:
        type: string
        description: Default bundle ID (apns-topic)
    use-sandbox:
        type: boolean
        description: Use sandbox endpoint instead of production
        defaultDescription: false
    token-refresh-interval:
        type: string
        description: How often to refresh the JWT (must be < 60m)
        defaultDescription: 50m
    request-timeout:
        type: string
        description: HTTP request timeout
        defaultDescription: 10s
    )");
}

auto Client::Send(const Notification& notification) const -> SendResult {
    return impl_->Send(notification);
}

auto Client::Send(const Credentials& credentials, const Notification& notification) const -> SendResult {
    return impl_->Send(credentials, notification);
}

}  // namespace apn
