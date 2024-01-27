#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

namespace views::v1::reverse_index {

class ReverseIndexResponse {
 public:
  ReverseIndexResponse(const std::string& employee_id)
      : employee_id(employee_id) {}

  std::string ToJSON() const;

  std::string employee_id;
};

class ReverseIndexRequest {
 public:
  ReverseIndexRequest(userver::storages::postgres::ClusterPtr cluster,
                      std::string& employee_id,
                      std::vector<std::optional<std::string>>&& fields)
      : cluster(cluster), employee_id(employee_id), fields(std::move(fields)) {}

  userver::storages::postgres::ClusterPtr cluster;
  std::string employee_id;
  std::vector<std::optional<std::string>> fields;
};

void AddReverseIndex(const ReverseIndexRequest& request);

}  // namespace views::v1::reverse_index