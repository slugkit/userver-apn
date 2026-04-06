#include <apn/types/credentials.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <fmt/format.h>

namespace apn {

auto Credentials::FromConfig(const userver::components::ComponentConfig& config) -> Credentials {
    return Credentials{
        .key_pem = config["key-pem"].As<std::string>(""),
        .key_pem_file = config["key-pem-file"].As<std::string>(""),
        .key_id = config["key-id"].As<std::string>(),
        .team_id = config["team-id"].As<std::string>(),
        .topic = config["topic"].As<std::string>(),
        .use_sandbox = config["use-sandbox"].As<bool>(false),
    };
}

auto Credentials::ResolvePem() const -> std::string {
    if (!key_pem.empty()) {
        return key_pem;
    }
    if (key_pem_file.empty()) {
        throw std::runtime_error(
            "apn-client: neither key-pem nor key-pem-file is configured "
            "(set APN_KEY_PEM or APN_KEY_PEM_FILE)"
        );
    }
    std::ifstream file{key_pem_file};
    if (!file) {
        throw std::runtime_error(
            fmt::format("apn-client: cannot open key-pem-file '{}'", key_pem_file)
        );
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        throw std::runtime_error(
            fmt::format("apn-client: error reading key-pem-file '{}'", key_pem_file)
        );
    }
    auto contents = buffer.str();
    if (contents.empty()) {
        throw std::runtime_error(
            fmt::format("apn-client: key-pem-file '{}' is empty", key_pem_file)
        );
    }
    return contents;
}

}  // namespace apn
