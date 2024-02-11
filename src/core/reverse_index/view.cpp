#include "view.hpp"

#include <nlohmann/json.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/engine/task/task.hpp>
#include <userver/engine/task/task_with_result.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include "userver/utils/async.hpp"

#include <queue>

using json = nlohmann::json;

namespace views::v1::reverse_index {

struct ValuesRow {
  std::vector<std::string> phones;
  std::optional<std::string> email, birthday, telegram_id, vk_id, team;
};

std::vector<std::optional<std::string>> GetEditFields(
    userver::storages::postgres::ClusterPtr pg_cluster, std::string employee_id,
    std::vector<std::string>& field_types) {
  auto result = pg_cluster->Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      "SELECT phones, email, birthday, telegram_id, vk_id, team "
      "FROM working_day.employees "
      "WHERE id = $1;",
      employee_id);

  auto values_res =
      result.AsSingleRow<ValuesRow>(userver::storages::postgres::kRowTag);

  std::vector<std::optional<std::string>> old_values;

  for (auto& field : field_types) {
    if (field == "phones") {
      old_values.insert(old_values.end(), values_res.phones.begin(),
                        values_res.phones.end());
    }
    if (field == "email") {
      old_values.push_back(values_res.email);
    }
    if (field == "birthday") {
      old_values.push_back(values_res.birthday);
    }
    if (field == "telegram_id") {
      old_values.push_back(values_res.telegram_id);
    }
    if (field == "vk_id") {
      old_values.push_back(values_res.vk_id);
    }
    if (field == "team") {
      old_values.push_back(values_res.team);
    }
  }

  return old_values;
}

struct AllValuesRow {
  std::string name, surname, role;
  std::optional<std::string> patronymic;
  std::vector<std::string> phones;
  std::optional<std::string> email, birthday, telegram_id, vk_id, team;
};

std::vector<std::optional<std::string>> GetAllFields(
    userver::storages::postgres::ClusterPtr pg_cluster,
    std::string employee_id) {
  auto result =
      pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                          "SELECT name, surname, role, patronymic, phones, "
                          "email, birthday, telegram_id, vk_id, team "
                          "FROM working_day.employees "
                          "WHERE id = $1;",
                          employee_id);

  auto values_res =
      result.AsSingleRow<AllValuesRow>(userver::storages::postgres::kRowTag);

  std::vector<std::optional<std::string>> old_values = {
      values_res.name,        values_res.surname, values_res.role,
      values_res.patronymic,  values_res.email,   values_res.birthday,
      values_res.telegram_id, values_res.vk_id,   values_res.team};

  old_values.insert(old_values.end(), values_res.phones.begin(),
                    values_res.phones.end());

  return old_values;
}

class ReverseIndexResponse {
 public:
  ReverseIndexResponse(const std::string& employee_id)
      : employee_id(employee_id) {}

  std::string ToJSON() const {
    nlohmann::json j;
    j["employee_id"] = employee_id;
    return j.dump();
  }

  std::string employee_id;
};

class ReverseIndex {
 public:
  void AddReverseIndex(const ReverseIndexRequest& request) {
    if (!request.cluster || request.fields.empty() ||
        request.employee_id.empty()) {
      LOG_WARNING() << "Invalid arguments for indexation";
      return;
    }

    auto tasks = tasks_.Lock();

    while (!tasks->empty() && tasks->front().IsFinished()) {
      tasks->pop();
    }

    tasks->push(userver::utils::AsyncBackground(
        "ReverseIndexAdd", userver::engine::current_task::GetTaskProcessor(),
        [this, r = std::move(request)]() -> ReverseIndexResponse {
          return this->AddReverseIndexFunc(r);
        }));
  }

  void DeleteReverseIndex(const ReverseIndexRequest& request) {
    if (!request.cluster || request.fields.empty() ||
        request.employee_id.empty()) {
      LOG_WARNING() << "Invalid arguments for indexation";
      return;
    }

    auto tasks = tasks_.Lock();

    while (!tasks->empty() && tasks->front().IsFinished()) {
      tasks->pop();
    }

    tasks->push(userver::utils::AsyncBackground(
        "ReverseIndexDelete", userver::engine::current_task::GetTaskProcessor(),
        [this, r = std::move(request)]() -> ReverseIndexResponse {
          return this->DeleteReverseIndexFunc(r);
        }));
  }

  void EditReverseIndex(EditIndexRequest& request) {
    if (!request.cluster || request.new_fields.empty() ||
        request.employee_id.empty()) {
      LOG_WARNING() << "Invalid arguments for indexation";
      return;
    }

    ReverseIndexRequest del_req(request.cluster, request.employee_id,
                                std::move(request.old_fields));
    ReverseIndexRequest add_req(request.cluster, request.employee_id,
                                std::move(request.new_fields));

    DeleteReverseIndex(del_req);
    AddReverseIndex(add_req);
  }

  void ClearTasks() {
    auto tasks = tasks_.Lock();

    while (!tasks->empty() && tasks->front().IsFinished()) {
      tasks->pop();
    }
  }
 
  static ReverseIndex& GetInstance() {
    static ReverseIndex instance;
    return instance;
  }

 private:
  ReverseIndex() = default;
  ~ReverseIndex() = default;

  ReverseIndex(const ReverseIndex&) = delete;
  ReverseIndex& operator=(const ReverseIndex&) = delete;

  ReverseIndexResponse AddReverseIndexFunc(
      const ReverseIndexRequest& request) const {
    for (auto& field : request.fields) {
      if (!field.has_value()) continue;

      auto result = request.cluster->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "INSERT INTO working_day.reverse_index(key, ids) "
          "VALUES ($1, ARRAY[$2]) "
          "ON CONFLICT (key) DO UPDATE SET ids = "
          "array_append(reverse_index.ids, "
          "$2);",
          field.value(), request.employee_id);
    }

    ReverseIndexResponse response(request.employee_id);

    return response;
  }

  ReverseIndexResponse DeleteReverseIndexFunc(
      const ReverseIndexRequest& request) const {
    for (auto& field : request.fields) {
      if (!field.has_value()) continue;

      auto result = request.cluster->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "UPDATE working_day.reverse_index "
          "SET ids = array_remove(ids, $2) "
          "WHERE key = $1; ",
          field.value(), request.employee_id);
    }

    ReverseIndexResponse response(request.employee_id);

    return response;
  }

  userver::concurrent::Variable<
      std::queue<userver::engine::TaskWithResult<ReverseIndexResponse>>>
      tasks_;
};

void AddReverseIndex(const ReverseIndexRequest& request) {
  return ReverseIndex::GetInstance().AddReverseIndex(request);
}

void DeleteReverseIndex(const ReverseIndexRequest& request) {
  return ReverseIndex::GetInstance().DeleteReverseIndex(request);
}

void EditReverseIndex(EditIndexRequest& request) {
  return ReverseIndex::GetInstance().EditReverseIndex(request);
}

void ClearTasks() {
  return ReverseIndex::GetInstance().ClearTasks();
}

}  // namespace views::v1::reverse_index