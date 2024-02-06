#include "custom_implicit_options.hpp"

namespace utils::custom_implicit_options {

CustomImplicitOptions::CustomImplicitOptions(const userver::components::ComponentConfig& config,
                                 const userver::components::ComponentContext& context,
                                 bool is_monitor)
    : ImplicitOptions(config, context, is_monitor) {}

std::string CustomImplicitOptions::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context) const {
    request.GetHttpResponse().SetHeader(static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    return ImplicitOptions::HandleRequestThrow(request, context);
}

}  // namespace utils::custom_implicit_options