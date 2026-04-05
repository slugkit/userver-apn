#include <apn/types/credentials.hpp>

namespace apn {

auto Credentials::FromConfig(const userver::components::ComponentConfig& config) -> Credentials {
    return Credentials{
        .key_pem = config["key-pem"].As<std::string>(),
        .key_id = config["key-id"].As<std::string>(),
        .team_id = config["team-id"].As<std::string>(),
        .topic = config["topic"].As<std::string>(),
        .use_sandbox = config["use-sandbox"].As<bool>(false),
    };
}

}  // namespace apn
