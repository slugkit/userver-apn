#pragma once

#include <string>
#include <string_view>

namespace apn::jwt {

auto GenerateToken(
    std::string_view key_pem,
    std::string_view key_id,
    std::string_view team_id
) -> std::string;

}  // namespace apn::jwt
