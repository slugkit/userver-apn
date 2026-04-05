# APNs Setup Guide

Complete guide to obtaining Apple credentials, configuring the component, and
deploying to production.

## 1. Apple Developer Account

You need a paid [Apple Developer Program](https://developer.apple.com/programs/)
membership ($99/year). Free accounts cannot send push notifications.

## 2. Register an App ID with Push Notifications

If you haven't already registered your app:

1. Go to [Certificates, Identifiers & Profiles](https://developer.apple.com/account/resources/identifiers/list).
2. Click **Identifiers** in the sidebar → **+** button.
3. Select **App IDs** → **App** → Continue.
4. Fill in:
   - **Description**: your app's name (e.g. "Poke Me").
   - **Bundle ID**: select **Explicit** and enter your bundle identifier
     (e.g. `com.yourcompany.pokeme`). This is the value you will use as the
     `APN_TOPIC` environment variable.
5. Scroll down to **Capabilities** and check **Push Notifications**.
6. Click **Continue** → **Register**.

If your App ID already exists, edit it and enable Push Notifications under
Capabilities.

## 3. Create an APNs Authentication Key

Apple allows a maximum of **two** APNs keys per developer account. Each key
works for **all apps** in the account — you do not need a separate key per app.

1. Go to [Keys](https://developer.apple.com/account/resources/authkeys/list)
   in your Apple Developer account.
2. Click **+** to create a new key.
3. Enter a **Key Name** (e.g. "APNs Production Key").
4. Check **Apple Push Notifications service (APNs)**.
5. Click **Continue** → **Register**.
6. **Download the `.p8` file.** Apple only lets you download it once. If you
   lose it, you must revoke the key and create a new one.
7. Note the **Key ID** displayed on the confirmation page (10-character
   alphanumeric string, e.g. `ABC123DEFG`). This is your `APN_KEY_ID`.

The downloaded file will be named `AuthKey_<KeyID>.p8`. It contains an ES256
(P-256 ECDSA) private key in PEM format:

```
-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg...
-----END PRIVATE KEY-----
```

## 4. Find Your Team ID

1. Go to [Apple Developer](https://developer.apple.com/account) → click your
   name in the top-right corner → **Membership Details** (or navigate to
   [Membership](https://developer.apple.com/account#MembershipDetailsCard)).
2. Your **Team ID** is a 10-character alphanumeric string (e.g. `XYZ9876543`).
   This is your `APN_TEAM_ID`.

## 5. Summary of Credentials

After completing the steps above you should have four values:

| Value | Example | Env Var |
|---|---|---|
| Auth key PEM content | `-----BEGIN PRIVATE KEY-----\nMIGH...` | `APN_KEY_PEM` |
| Key ID | `ABC123DEFG` | `APN_KEY_ID` |
| Team ID | `XYZ9876543` | `APN_TEAM_ID` |
| Bundle ID (topic) | `com.yourcompany.pokeme` | `APN_TOPIC` |

## 6. Environment Variables

All credentials are passed via environment variables. **Never bake credentials
into Docker images, config files checked into git, or build artifacts.**

### Required

| Variable | Description |
|---|---|
| `APN_KEY_PEM` | Full PEM content of the `.p8` file, including the `-----BEGIN/END PRIVATE KEY-----` lines |
| `APN_KEY_ID` | 10-character Key ID from Apple Developer portal |
| `APN_TEAM_ID` | 10-character Team ID from your Apple Developer account |
| `APN_TOPIC` | App bundle ID (e.g. `com.yourcompany.pokeme`). This is sent as the `apns-topic` header |

### Reading the `.p8` file into an env var

The PEM content contains newlines. Most secret management systems handle
multi-line values natively. If you need to set it manually:

```bash
# From the file directly (preserves newlines)
export APN_KEY_PEM="$(cat AuthKey_ABC123DEFG.p8)"

# Verify it looks right
echo "$APN_KEY_PEM"
# Should print:
# -----BEGIN PRIVATE KEY-----
# MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg...
# -----END PRIVATE KEY-----
```

## 7. Service Configuration

The component reads credentials from environment variables via userver's
`#env` config suffix. Add to your service's `static_config.yaml`:

```yaml
apn-client:
    key-pem: ""
    key-pem#env: APN_KEY_PEM
    key-id: ""
    key-id#env: APN_KEY_ID
    team-id: ""
    team-id#env: APN_TEAM_ID
    topic: ""
    topic#env: APN_TOPIC
    use-sandbox: $apn-use-sandbox
    token-refresh-interval: 50m
    request-timeout: 10s
```

And in `config_vars.yaml`:

```yaml
# Development
apn-use-sandbox: true

# Production
# apn-use-sandbox: false
```

The `#env` properties override the static values at startup. The empty string
defaults ensure the config is valid even if the env var is not set (the
component will fail at runtime with a clear error if credentials are missing).

## 8. Local Development

For local development inside the devcontainer, create a `.env` file in the
project root (already gitignored):

```bash
# .env — local development only, never commit this file
APN_KEY_PEM="-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg...
-----END PRIVATE KEY-----"
APN_KEY_ID=ABC123DEFG
APN_TEAM_ID=XYZ9876543
APN_TOPIC=com.yourcompany.pokeme
```

Then either `source .env` before running the service, or pass the env file to
docker compose:

```yaml
# docker-compose.yaml
services:
  your-service:
    env_file:
      - .env
```

## 9. Production Deployment

### Docker / fly.io

Set secrets through your cloud provider's secrets management. Never store
credentials in the Docker image or Dockerfile.

**fly.io:**

```bash
fly secrets set APN_KEY_PEM="$(cat AuthKey_ABC123DEFG.p8)"
fly secrets set APN_KEY_ID=ABC123DEFG
fly secrets set APN_TEAM_ID=XYZ9876543
fly secrets set APN_TOPIC=com.yourcompany.pokeme
```

**Docker Compose (production):**

Use Docker secrets or an env file that is not part of the image:

```yaml
services:
  api:
    image: your-registry/pokeme-api:latest
    env_file:
      - /run/secrets/apn.env    # mounted by orchestrator
```

**Kubernetes:**

Create a Secret and reference it in your deployment:

```bash
kubectl create secret generic apn-credentials \
    --from-file=APN_KEY_PEM=AuthKey_ABC123DEFG.p8 \
    --from-literal=APN_KEY_ID=ABC123DEFG \
    --from-literal=APN_TEAM_ID=XYZ9876543 \
    --from-literal=APN_TOPIC=com.yourcompany.pokeme
```

```yaml
# deployment.yaml
containers:
  - name: api
    envFrom:
      - secretRef:
          name: apn-credentials
```

### AWS ECS

Use AWS Secrets Manager or SSM Parameter Store:

```json
{
    "containerDefinitions": [{
        "secrets": [
            { "name": "APN_KEY_PEM", "valueFrom": "arn:aws:secretsmanager:region:account:secret:apn-key-pem" },
            { "name": "APN_KEY_ID", "valueFrom": "arn:aws:ssm:region:account:parameter/apn/key-id" },
            { "name": "APN_TEAM_ID", "valueFrom": "arn:aws:ssm:region:account:parameter/apn/team-id" },
            { "name": "APN_TOPIC", "valueFrom": "arn:aws:ssm:region:account:parameter/apn/topic" }
        ]
    }]
}
```

## 10. Sandbox vs Production

APNs has two environments:

| Environment | Hostname | When to use |
|---|---|---|
| Sandbox | `api.sandbox.push.apple.com` | Development builds, TestFlight |
| Production | `api.push.apple.com` | App Store builds |

Set `use-sandbox: true` in development and `use-sandbox: false` in production
(via `config_vars.yaml`). The same auth key works for both environments.

**How to tell which environment a device token belongs to:**

- Tokens from Xcode debug builds and TestFlight → sandbox.
- Tokens from App Store downloads → production.
- Sending a production token to sandbox (or vice versa) returns HTTP 400
  `BadDeviceToken`.

## 11. Key Rotation

APNs auth keys do not expire. However, you may want to rotate them
periodically:

1. Create a new key in Apple Developer → Keys (you can have two active keys).
2. Update `APN_KEY_PEM` and `APN_KEY_ID` in your secrets store.
3. Deploy. The component picks up new env vars on restart.
4. Once all instances are using the new key, revoke the old one in Apple
   Developer portal.

There is no downtime during rotation if you deploy the new key before revoking
the old one.

## 12. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| HTTP 403 `ExpiredProviderToken` | JWT older than 60 minutes | Decrease `token-refresh-interval` (default 50m should be fine) |
| HTTP 403 `InvalidProviderToken` | Wrong key, team ID, or malformed JWT | Verify `APN_KEY_PEM`, `APN_KEY_ID`, `APN_TEAM_ID` |
| HTTP 400 `BadDeviceToken` | Token doesn't match environment | Sandbox token → sandbox endpoint, production token → production endpoint |
| HTTP 400 `TopicDisallowed` | Bundle ID not enabled for push | Enable Push Notifications on App ID in Apple Developer portal |
| HTTP 400 `MissingTopic` | `apns-topic` header not set | Set `APN_TOPIC` env var to your bundle ID |
| Connection refused | HTTP/2 not working | Verify the service's HTTP client supports HTTP/2 (`HttpVersion::k2Tls`) |
| `status_code = 0` in `SendResult` | Network/TLS/timeout error — request never reached APNs | Check connectivity to `api.push.apple.com:443`, DNS resolution, TLS config |
| Startup crash: `key-pem is not configured` | `APN_KEY_PEM` env var not set or not passed to container | Check secrets config, `docker inspect`, `fly secrets list` |
| Startup crash: `invalid key-pem` | `APN_KEY_PEM` is not a valid PKCS#8 PEM private key | Verify the `.p8` file content, must start with `-----BEGIN PRIVATE KEY-----` |
