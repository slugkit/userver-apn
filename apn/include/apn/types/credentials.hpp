#pragma once

#include <string>

#include <userver/components/component_config.hpp>

namespace apn {

struct Credentials {
    std::string key_pem;
    std::string key_id;
    std::string team_id;
    std::string topic;
    bool use_sandbox = false;

    static auto FromConfig(const userver::components::ComponentConfig& config) -> Credentials;
};

}  // namespace apn
