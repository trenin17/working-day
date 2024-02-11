#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>

namespace views::v1::clear_tasks {

void AppendClearTasks(userver::components::ComponentList& component_list);

}