#pragma once

#include <userver/server/middlewares/http_middleware_base.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/components/component_list.hpp>

namespace middleware::middleware {

void AppendRequestCaptureMiddleware(userver::components::ComponentList& component_list);

}  // namespace middleware