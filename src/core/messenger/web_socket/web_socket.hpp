#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>


namespace core::websocket {

void AppendWebSocket(userver::components::ComponentList& component_list);

}  // namespace websocket
