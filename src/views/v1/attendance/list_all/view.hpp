#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>

namespace views::v1::attendance::list_all {

void AppendAttendanceListAll(
    userver::components::ComponentList& component_list);

}  // namespace views::v1::attendance::list_all
