#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>

namespace views::v1::messenger::recent_messages {

void AppendRecentMessages(userver::components::ComponentList& component_list);

}  // namespace views::v1::messenger::recent_messages
