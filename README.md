# userver-apn

A userver component for sending push notifications via Apple Push Notification
service (APNs). Designed to be plugged into any userver-based project as a
library.

## Overview

The library provides:

- **`apn::Client`** — a userver component that handles APNs authentication
  (JWT token-based) and push delivery over HTTP/2.
- **`apn::Credentials`** — credential set that can be loaded from component
  config or constructed programmatically (e.g. from a database for
  multi-tenant setups).
- **`apn::handlers::DebugSend`** — an optional HTTP handler for sending
  test pushes using the component's default credentials.

For Apple Developer account setup, credential management, and deployment
configuration see [SETUP.md](SETUP.md).

## Apple Push Notification Service

### API

APNs is an HTTP/2 API. Every push is a single request:

```
POST /3/device/{device_token}
Host: api.push.apple.com          (production)
      api.sandbox.push.apple.com  (sandbox)
```

**Required headers:**

| Header | Description |
|---|---|
| `authorization` | `bearer <jwt>` — provider authentication token |
| `apns-topic` | App bundle ID (e.g. `com.example.app`) |
| `apns-push-type` | `alert`, `background`, `voip`, `complication`, `fileprovider`, `mdm`, `live-activity` |

**Optional headers:**

| Header | Description |
|---|---|
| `apns-priority` | `10` (immediate, default for alert) or `5` (power-saving, default for background) |
| `apns-expiration` | UNIX timestamp. `0` = deliver once or drop |
| `apns-collapse-id` | Replaces earlier notification with same ID (max 64 bytes) |
| `apns-id` | Canonical UUID for the notification. APNs returns one if omitted |

**Request body** — JSON payload (max 4 KB for regular, 5 KB for VoIP):

```json
{
    "aps": {
        "alert": {
            "title": "Title",
            "body": "Message body"
        },
        "badge": 1,
        "sound": "default"
    }
}
```

**Response** — status code + JSON body on error:

| Status | Meaning |
|---|---|
| 200 | Success |
| 400 | Bad request (malformed JSON, missing headers) |
| 403 | Authentication error (bad token, wrong topic) |
| 404 | Bad device token |
| 410 | Device token is no longer active |
| 429 | Too many requests for this device token |
| 500 | Internal APNs server error |
| 503 | Server unavailable, retry later |

Error body: `{"reason": "BadDeviceToken"}` (see
[Apple docs](https://developer.apple.com/documentation/usernotifications/handling-notification-responses-from-apns)
for full list).

### Authentication

APNs supports two authentication methods. This library uses **token-based
authentication** (the modern, recommended approach).

**Credentials required:**

1. **Auth Key** — a `.p8` file containing an ES256 (P-256) private key.
   Each key has a **Key ID** (10-character string).
2. **Team ID** — 10-character identifier from your Apple Developer account.
3. **Bundle ID** — your app's bundle identifier (used as `apns-topic`).

**How it works:**

The client generates a short-lived JWT (1 hour max, refreshed at a
configurable interval) signed with ES256:

```
Header:  {"alg": "ES256", "kid": "<key-id>"}
Payload: {"iss": "<team-id>", "iat": <unix-timestamp>}
```

The token is sent as `Authorization: bearer <jwt>` on every push request.
Apple requires refreshing the token at least every 60 minutes; sending a
stale token results in HTTP 403 `ExpiredProviderToken`.

## Component Design

### `apn::Credentials`

Holds the four values needed to authenticate with APNs. Can be created from
component config (for the service's own credentials) or constructed
programmatically (for multi-tenant scenarios where credentials come from a
database).

```cpp
namespace apn {

struct Credentials {
    std::string key_pem;        // PEM content of the .p8 file
    std::string key_id;         // 10-character Key ID
    std::string team_id;        // 10-character Team ID
    std::string topic;          // default bundle ID (apns-topic)
    bool use_sandbox = false;   // sandbox vs production endpoint

    /// Load from a userver ComponentConfig (reads key-pem, key-id, etc.).
    static auto FromConfig(const userver::components::ComponentConfig& config)
        -> Credentials;
};

}  // namespace apn
```

### `apn::Client`

A `userver::components::ComponentBase` that owns the default credential set
and exposes `Send()`.

**Dependencies:**

- `userver::components::HttpClient` — the underlying HTTP client (must be
  registered in the service's component list).

**Public interface:**

```cpp
namespace apn {

struct Notification {
    std::string device_token;
    std::string payload;                              // JSON string
    std::string topic;                                // bundle ID (optional, uses default from credentials)
    std::string push_type = "alert";                  // alert | background | voip | ...
    std::int32_t priority = 10;                       // 10 = immediate, 5 = power-saving
    std::int32_t expiration = 0;                      // UNIX timestamp, 0 = deliver once
    std::string collapse_id;                          // optional
};

struct SendResult {
    std::int32_t status_code;                         // 200 = success, 400-503 = APNs error, 0 = transport error
    std::string apns_id;                              // UUID assigned by APNs (empty on transport error)
    std::string reason;                               // empty on success, APNs reason or error message
};

class Client final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "apn-client";

    Client(const ComponentConfig&, const ComponentContext&);
    ~Client();

    static auto GetStaticConfigSchema() -> userver::yaml_config::Schema;

    /// Send using the component's default credentials (from config / env vars).
    [[nodiscard]] auto Send(const Notification& notification) const -> SendResult;

    /// Send using explicit credentials (for multi-tenant setups).
    /// The caller provides credentials loaded from a database or other source.
    /// Each call generates a fresh JWT — no token caching across calls.
    [[nodiscard]] auto Send(
        const Credentials& credentials,
        const Notification& notification
    ) const -> SendResult;

private:
    struct Impl;
    userver::utils::FastPimpl<Impl, kImplSize, kImplAlign> impl_;
};

}  // namespace apn
```

The single-argument `Send()` uses the component's default credentials and
benefits from JWT caching (tokens are reused until they approach expiry). The
two-argument overload generates a fresh JWT per call — suitable for
multi-tenant scenarios where each tenant has their own Apple credentials.

### `apn::handlers::DebugSend`

An optional HTTP handler included in the library for testing pushes. It uses
the component's default credentials — no private keys or credentials are
accepted via the HTTP request.

```cpp
namespace apn::handlers {

class DebugSend final : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "apn-handler-debug-send";

    DebugSend(const ComponentConfig&, const ComponentContext&);

    auto HandleRequestJsonThrow(
        const HttpRequest& request,
        const json::Value& request_json,
        RequestContext& context
    ) const -> json::Value override;

    static auto GetStaticConfigSchema() -> userver::yaml_config::Schema;
};

}  // namespace apn::handlers
```

**Request** — `POST` with JSON body:

```json
{
    "device_token": "a1b2c3d4...",
    "payload": {
        "aps": {
            "alert": { "title": "Test", "body": "Hello from debug handler" }
        }
    }
}
```

The `payload` field is the raw APNs JSON payload — the handler serialises it
to a string and passes it to `Client::Send()`.

**Optional fields:**

| Field | Type | Default | Description |
|---|---|---|---|
| `topic` | string | component default | Override `apns-topic` |
| `push_type` | string | `"alert"` | `alert`, `background`, `voip`, etc. |
| `priority` | int | `10` | `10` or `5` |
| `expiration` | int | `0` | UNIX timestamp |
| `collapse_id` | string | empty | Collapse identifier |

**Response:**

```json
{
    "status_code": 200,
    "apns_id": "uuid-assigned-by-apple",
    "reason": ""
}
```

**Static config:**

```yaml
apn-handler-debug-send:
    path: /apn/debug/send
    method: POST
    task_processor: main-task-processor
    apn-client: apn-client                  # component name, default: "apn-client"
```

The path is configurable — the consumer decides where to mount it (or omits
it from the component list entirely).

### Internal Architecture

```
Client
├── Impl
│   ├── http_client_       → userver HttpClient (from component context)
│   ├── credentials_       → default Credentials (from config / env vars)
│   ├── base_url_          → sandbox or production endpoint
│   ├── request_timeout_   → configurable (default: 10s)
│   ├── token_refresh_interval_ → configurable (default: 50 min)
│   ├── token              → rcu::Variable<string> (lock-free reads)
│   └── refresh_task       → PeriodicTask (background JWT refresh)
│
├── Send(notification)                        → reads cached JWT via RCU
│   ├── token.Read()
│   ├── Build HTTP/2 request
│   └── Parse response
│
├── Send(credentials, notification)           → fresh JWT per call
│   ├── GenerateToken(credentials)
│   ├── Build HTTP/2 request
│   └── Parse response
│
└── RefreshToken()                            → called by PeriodicTask
    ├── Sign new JWT with ES256
    ├── token.Assign(new_jwt)
    └── Reschedule refresh_task
```

**Token management:**

- The JWT is stored in an `rcu::Variable<std::string>` for lock-free reads
  from concurrent `Send()` calls.
- A `PeriodicTask` runs `RefreshToken()` at the configured interval
  (default 50 minutes, must be less than Apple's 60-minute limit).
- The initial JWT is generated synchronously in the constructor. If the key
  is invalid, the component fails to start.

**JWT generation** uses userver's built-in crypto:

- `crypto::SignerEs256` for ES256 signing (RFC 7518 format)
- `crypto::base64::Base64UrlEncode` with `Pad::kWithout` for JWT encoding
- `userver::utils::datetime::Now()` for `iat` claim

**HTTP request** uses userver's HTTP client:

- `http_version(HttpVersion::k2Tls)` — HTTP/2 over TLS (required by APNs)
- Standard `post()` with JSON body
- Headers: `authorization`, `apns-topic`, `apns-push-type`, `apns-priority`,
  `apns-expiration`, `apns-collapse-id`

### Configuration

All credentials are passed via environment variables. The `#env` suffix in
userver's YAML config resolves a property from the environment at startup.

```yaml
# In the service's static_config.yaml

apn-client:
    # Required — all sourced from environment variables
    key-pem: ""
    key-pem#env: APN_KEY_PEM
    key-id: ""
    key-id#env: APN_KEY_ID
    team-id: ""
    team-id#env: APN_TEAM_ID
    topic: ""
    topic#env: APN_TOPIC

    # Optional
    use-sandbox: $apn-use-sandbox             # true for dev, false for production
    token-refresh-interval: 50m               # must be < 60m
    request-timeout: 10s
```

The `key-pem` property accepts the raw PEM content of the `.p8` file
(including `-----BEGIN PRIVATE KEY-----` and `-----END PRIVATE KEY-----`
lines). This avoids mounting secret files into containers.

### Service Integration

**CMakeLists.txt** (consumer project):

```cmake
add_subdirectory(third-party/userver-apn/apn)
target_link_libraries(your-service PRIVATE apn_client)
```

**main.cpp** (register components):

```cpp
#include <apn/components/client.hpp>
#include <apn/handlers/debug_send.hpp>      // optional

auto component_list = userver::components::MinimalServerComponentList()
    .Append<userver::components::HttpClient>()     // required dependency
    .Append<userver::clients::dns::Component>()    // required by HttpClient
    .Append<apn::Client>()
    .Append<apn::handlers::DebugSend>()            // optional debug handler
    // ... your handlers
    ;
```

**Sending with default credentials:**

```cpp
auto& client = context.FindComponent<apn::Client>();

apn::Notification notification;
notification.device_token = "a1b2c3d4...";
notification.payload = R"({"aps":{"alert":{"title":"Hi","body":"Hello"}}})";

auto result = client.Send(notification);
```

**Sending with per-tenant credentials (multi-tenant):**

```cpp
auto& client = context.FindComponent<apn::Client>();

// Credentials loaded from database, per-tenant config, etc.
apn::Credentials creds;
creds.key_pem = tenant.apn_key_pem;
creds.key_id = tenant.apn_key_id;
creds.team_id = tenant.apn_team_id;
creds.topic = tenant.bundle_id;
creds.use_sandbox = false;

apn::Notification notification;
notification.device_token = device.token;
notification.payload = BuildPayload(message);

auto result = client.Send(creds, notification);
```

## Library Structure

```
apn/
├── CMakeLists.txt
├── include/apn/
│   ├── components/
│   │   └── client.hpp          # Client component
│   ├── handlers/
│   │   └── debug_send.hpp      # Debug send handler
│   └── types/
│       ├── credentials.hpp     # Credentials struct
│       ├── notification.hpp    # Notification struct
│       └── result.hpp          # SendResult struct
├── src/apn/
│   ├── components/
│   │   └── client.cpp          # Client implementation
│   ├── handlers/
│   │   └── debug_send.cpp      # Debug handler implementation
│   └── jwt/
│       ├── token.hpp           # JWT generation
│       └── token.cpp
└── tests/
    ├── jwt_test.cpp            # JWT generation tests
    └── notification_test.cpp   # Serialization tests
```

## Dependencies

- **userver** (core, universal) — HTTP client, crypto, JSON, logging
- **No external dependencies** — all crypto (ES256, base64url) is provided by
  userver's built-in wrappers around OpenSSL

## License

Apache License 2.0
