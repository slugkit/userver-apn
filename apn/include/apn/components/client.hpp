#pragma once

#include <apn/types/credentials.hpp>
#include <apn/types/notification.hpp>
#include <apn/types/result.hpp>

#include <userver/components/component_base.hpp>
#include <userver/utils/fast_pimpl.hpp>
#include <userver/yaml_config/schema.hpp>

namespace apn {

class Client final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "apn-client";

    Client(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );
    ~Client();

    static auto GetStaticConfigSchema() -> userver::yaml_config::Schema;

    [[nodiscard]] auto Send(const Notification& notification) const -> SendResult;

    [[nodiscard]] auto Send(
        const Credentials& credentials,
        const Notification& notification
    ) const -> SendResult;

private:
    constexpr static auto kImplSize = 752UL;
    constexpr static auto kImplAlign = 16UL;
    struct Impl;
    userver::utils::FastPimpl<Impl, kImplSize, kImplAlign> impl_;
};

}  // namespace apn
