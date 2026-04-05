#pragma once

#include <cstdint>
#include <string>

namespace apn {

struct Notification {
    std::string device_token;
    std::string payload;
    std::string topic;
    std::string push_type = "alert";
    std::int32_t priority = 10;
    std::int32_t expiration = 0;
    std::string collapse_id;
};

}  // namespace apn
