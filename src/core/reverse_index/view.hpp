#pragma once

#define USE_REVERSE_INDEX

#include <initializer_list>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include <userver/components/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include "definitions/all.hpp"

namespace views::v1::reverse_index {

void ReverseIndexHandler(const ReverseIndexRequest& request, userver::storages::postgres::ClusterPtr cluster,
                        const EmployeeAllData& data);

void ClearTasks();

}  // namespace views::v1::reverse_index