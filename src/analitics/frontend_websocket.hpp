#pragma once

#include <userver/utest/using_namespace_userver.hpp>

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/websocket/websocket_handler.hpp>

namespace analitics::websocket {

void AppendRequestFrontendWebsockets(userver::components::ComponentList& component_list);

}  // namespace analitics::websocket