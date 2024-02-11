#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

namespace views::v1::reverse_index {

std::vector<std::optional<std::string>> GetEditFields(
    userver::storages::postgres::ClusterPtr pg_cluster, std::string employee_id,
    std::vector<std::string>& field_types);

std::vector<std::optional<std::string>> GetAllFields(
    userver::storages::postgres::ClusterPtr pg_cluster, std::string employee_id);

class ReverseIndexRequest {
 public:
  ReverseIndexRequest(userver::storages::postgres::ClusterPtr cluster,
                      std::string employee_id,
                      std::vector<std::optional<std::string>>&& fields)
      : cluster(cluster), employee_id(employee_id), fields(std::move(fields)) {}

  userver::storages::postgres::ClusterPtr cluster;
  std::string employee_id;
  std::vector<std::optional<std::string>> fields;
};

class EditIndexRequest {
 public:
  EditIndexRequest(userver::storages::postgres::ClusterPtr cluster,
                   std::string employee_id,
                   std::vector<std::optional<std::string>>&& old_fields,
                   std::vector<std::optional<std::string>>&& new_fields)
      : cluster(cluster),
        employee_id(employee_id),
        old_fields(std::move(old_fields)),
        new_fields(std::move(new_fields)) {}

  userver::storages::postgres::ClusterPtr cluster;
  std::string employee_id;
  std::vector<std::optional<std::string>> old_fields;
  std::vector<std::optional<std::string>> new_fields;
};

void AddReverseIndex(const ReverseIndexRequest& request);

void DeleteReverseIndex(const ReverseIndexRequest& request);

void EditReverseIndex(EditIndexRequest& request);

void ClearTasks();

}  // namespace views::v1::reverse_index