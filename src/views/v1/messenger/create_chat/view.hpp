#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>

namespace views::v1::messenger::create {

void AppendCreateChat(userver::components::ComponentList& component_list);

}  // namespace views::v1::messenger::create
