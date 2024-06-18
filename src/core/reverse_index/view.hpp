#pragma once

#include <codecvt>
#include <initializer_list>
#include <locale>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include <userver/components/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include <core/json_compatible/struct.hpp>

namespace core::reverse_index {

class EmployeeAllData {
 public:
  std::string employee_id;
  std::optional<std::string> name, surname, patronymic, role, email, birthday,
      telegram_id, vk_id, team, company_id;
  std::optional<std::vector<std::string>> phones;
};

std::string ConvertToLower(std::string s);

class ReverseIndexResponse : public JsonCompatible {
 public:
  ReverseIndexResponse(const std::string& worker_id) {
    employee_id = worker_id;
  }

  REGISTER_STRUCT_FIELD(employee_id, std::string, "employee_id");
};

class ReverseIndexRequest {
 public:
  std::function<ReverseIndexResponse()> func;
};

void ReverseIndexHandler(const ReverseIndexRequest& request);

void ClearTasks();

}  // namespace core::reverse_index