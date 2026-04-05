#include <apn/handlers/debug_send.hpp>

#include <apn/components/client.hpp>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace apn::handlers {

namespace uhandlers = userver::server::handlers;
namespace json = userver::formats::json;

struct DebugSend::Impl {
    const Client& apn_client;

    Impl(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
        : apn_client(context.FindComponent<Client>(config["apn-client"].As<std::string>("apn-client"))) {}
};

DebugSend::DebugSend(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : BaseType{config, context}
    , impl_{config, context} {}

DebugSend::~DebugSend() = default;

auto DebugSend::HandleRequestJsonThrow(
    [[maybe_unused]] const userver::server::http::HttpRequest& request,
    const json::Value& request_json,
    [[maybe_unused]] userver::server::request::RequestContext& context
) const -> json::Value {
    if (!request_json.HasMember("device_token")) {
        throw uhandlers::ClientError(
            uhandlers::InternalMessage{"Missing required field: device_token"},
            uhandlers::ExternalBody{R"({"error":"missing field: device_token"})"}
        );
    }
    if (!request_json.HasMember("payload")) {
        throw uhandlers::ClientError(
            uhandlers::InternalMessage{"Missing required field: payload"},
            uhandlers::ExternalBody{R"({"error":"missing field: payload"})"}
        );
    }

    Notification notification;
    notification.device_token = request_json["device_token"].As<std::string>();
    notification.payload = json::ToString(request_json["payload"]);

    if (request_json.HasMember("topic")) {
        notification.topic = request_json["topic"].As<std::string>();
    }
    if (request_json.HasMember("push_type")) {
        notification.push_type = request_json["push_type"].As<std::string>();
    }
    if (request_json.HasMember("priority")) {
        notification.priority = request_json["priority"].As<std::int32_t>();
    }
    if (request_json.HasMember("expiration")) {
        notification.expiration = request_json["expiration"].As<std::int32_t>();
    }
    if (request_json.HasMember("collapse_id")) {
        notification.collapse_id = request_json["collapse_id"].As<std::string>();
    }

    LOG_INFO() << "Debug APNs send to device_token=" << notification.device_token;

    auto result = impl_->apn_client.Send(notification);

    json::ValueBuilder response;
    response["status_code"] = result.status_code;
    response["apns_id"] = result.apns_id;
    response["reason"] = result.reason;

    if (result.status_code != 200 && result.status_code != 0) {
        auto& http_response = request.GetHttpResponse();
        http_response.SetStatus(static_cast<userver::server::http::HttpStatus>(result.status_code));
    } else if (result.status_code == 0) {
        auto& http_response = request.GetHttpResponse();
        http_response.SetStatus(userver::server::http::HttpStatus::kBadGateway);
    }

    return response.ExtractValue();
}

auto DebugSend::GetStaticConfigSchema() -> userver::yaml_config::Schema {
    return userver::yaml_config::MergeSchemas<BaseType>(R"(
type: object
description: Debug handler for sending APNs push notifications
additionalProperties: false
properties:
    apn-client:
        type: string
        description: Component name for the APNs client
        defaultDescription: apn-client
    )");
}

}  // namespace apn::handlers
