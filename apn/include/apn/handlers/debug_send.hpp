#pragma once

#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/utils/fast_pimpl.hpp>

namespace apn::handlers {

class DebugSend final : public userver::server::handlers::HttpHandlerJsonBase {
public:
    using BaseType = userver::server::handlers::HttpHandlerJsonBase;
    static constexpr std::string_view kName = "apn-handler-debug-send";

    DebugSend(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );
    ~DebugSend() override;

    static auto GetStaticConfigSchema() -> userver::yaml_config::Schema;

    auto HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_json,
        userver::server::request::RequestContext& context
    ) const -> userver::formats::json::Value override;

private:
    constexpr static auto kImplSize = 8UL;
    constexpr static auto kImplAlign = 8UL;
    struct Impl;
    userver::utils::FastPimpl<Impl, kImplSize, kImplAlign> impl_;
};

}  // namespace apn::handlers
