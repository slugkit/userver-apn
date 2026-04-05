#pragma once

#include <cstdint>
#include <string>

namespace apn {

struct SendResult {
    /// HTTP status from APNs (200 on success, 400-503 on error),
    /// or 0 if the request never reached APNs (network/TLS/timeout).
    std::int32_t status_code;
    std::string apns_id;
    std::string reason;
};

}  // namespace apn
