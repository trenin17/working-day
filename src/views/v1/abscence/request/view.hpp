#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>

namespace views::v1::abscence::request {

void AppendAbscenceRequest(userver::components::ComponentList& component_list);

}  // namespace views::v1::abscence::request