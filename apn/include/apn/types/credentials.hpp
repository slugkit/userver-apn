#pragma once

#include <string>

#include <userver/components/component_config.hpp>

namespace apn {

struct Credentials {
    std::string key_pem;
    std::string key_pem_file;
    std::string key_id;
    std::string team_id;
    std::string topic;
    bool use_sandbox = false;

    static auto FromConfig(const userver::components::ComponentConfig& config) -> Credentials;

    /// Returns the effective PEM contents.
    /// If `key_pem` is non-empty, it is returned as-is.
    /// Otherwise `key_pem_file` is read from disk.
    /// Throws `std::runtime_error` if neither is set or the file is unreadable.
    auto ResolvePem() const -> std::string;
};

}  // namespace apn
