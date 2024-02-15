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

class ReverseIndex {
public:
  void ReverseIndexHandler(const ReverseIndexRequest& request) {
    if (!request.cluster || request.employee_id.empty()) {
      LOG_WARNING() << "Invalid arguments for indexation";
      return;
    }

    auto tasks = tasks_.Lock();

    while (!tasks->empty() && tasks->front().IsFinished()) {
      tasks->pop();
    }

    tasks->push(userver::utils::AsyncBackground(
        "ReverseIndexFunc", userver::engine::current_task::GetTaskProcessor(),
        request.func, std::move(request)));
  }

  ReverseIndexResponse AddReverseIndex(const ReverseIndexRequest& request) {
    return AddReverseIndexFunc(request);
  }

  ReverseIndexResponse DeleteReverseIndex(const ReverseIndexRequest& request) {
    return DeleteReverseIndexFunc(request);
  }

  ReverseIndexResponse EditReverseIndex(const ReverseIndexRequest& request) {
    return EditReverseIndexFunc(request);
  }

  void ClearTasks() {
    auto tasks = tasks_.Lock();

    while (!tasks->empty()) {
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

  userver::concurrent::Variable<
      std::queue<userver::engine::TaskWithResult<ReverseIndexResponse>>>
      tasks_;

  ReverseIndexResponse AddReverseIndexFunc(const ReverseIndexRequest& request) {
    
    std::vector<std::optional<std::string>> fields = {
        request.name, request. surname, request.patronymic,
        request.role, request.email, request.birthday,
        request.telegram_id, request.vk_id, request.team
    };

    if (request.phones.has_value()) {
      fields.insert(fields.end(), request.phones.value().begin(), request.phones.value().end());
    }
    
    for (auto& field : fields ) {
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

  struct AllValuesRow {
    std::string name, surname, role;
    std::optional<std::string> patronymic;
    std::vector<std::string> phones;
    std::optional<std::string> email, birthday, telegram_id, vk_id, team;
  };

  ReverseIndexResponse DeleteReverseIndexFunc(
      const ReverseIndexRequest& request)  {
    auto result =
        request.cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                            "SELECT name, surname, role, patronymic, phones, "
                            "email, birthday, telegram_id, vk_id, team "
                            "FROM working_day.employees "
                            "WHERE id = $1;",
                            request.employee_id);

    auto values_res =
        result.AsSingleRow<AllValuesRow>(userver::storages::postgres::kRowTag);

    std::vector<std::optional<std::string>> old_values = {
        values_res.name,        values_res.surname, values_res.role,
        values_res.patronymic,  values_res.email,   values_res.birthday,
        values_res.telegram_id, values_res.vk_id,   values_res.team
    };

    old_values.insert(old_values.end(), values_res.phones.begin(), values_res.phones.end());

    for (auto& field : old_values) {
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

  struct EditValuesRow {
    std::optional<std::vector<std::string>> phones;
    std::optional<std::string> email, birthday, telegram_id, vk_id, team;
  };

  ReverseIndexResponse EditReverseIndexFunc(
      const ReverseIndexRequest& request)  {
    
    auto grab_result = request.cluster->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT CASE WHEN $2 IS NULL THEN NULL ELSE phones END, "
          "CASE WHEN $3 IS NULL THEN NULL ELSE email END, "
          "CASE WHEN $4 IS NULL THEN NULL ELSE birthday END, "
          "CASE WHEN $5 IS NULL THEN NULL ELSE telegram_id END, "
          "CASE WHEN $6 IS NULL THEN NULL ELSE vk_id END, "
          "CASE WHEN $7 IS NULL THEN NULL ELSE team END "
          "FROM working_day.employees "
          "WHERE id = $1; ",
          request.employee_id, request.phones, request.email, request.birthday,
          request.telegram_id, request.vk_id, request.team);
    
    auto old_values = grab_result.AsSingleRow<EditValuesRow>(userver::storages::postgres::kRowTag);

    std::vector<std::optional<std::string>> old_values_vec = {
                                    old_values.email, old_values.birthday, old_values.telegram_id,
                                    old_values.vk_id, old_values.team};
    
    if (old_values.phones.has_value()) {
      old_values_vec.insert(old_values_vec.end(), old_values.phones.value().begin(), old_values.phones.value().end());
    }

    for (auto& field : old_values_vec) {
      if (!field.has_value()) continue;

      auto result = request.cluster->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "UPDATE working_day.reverse_index "
          "SET ids = array_remove(ids, $2) "
          "WHERE key = $1; ",
          field.value(), request.employee_id);
    }

    const ReverseIndexRequest add_request = request;
    AddReverseIndexFunc(add_request);

    ReverseIndexResponse response(request.employee_id);
    return response;
  }
};

void ReverseIndexHandler(const ReverseIndexRequest& request) {
  return ReverseIndex::GetInstance().ReverseIndexHandler(request);
}

ReverseIndexResponse AddReverseIndex(const ReverseIndexRequest& request) {
  return ReverseIndex::GetInstance().AddReverseIndex(request);
}

ReverseIndexResponse DeleteReverseIndex(const ReverseIndexRequest& request) {
  return ReverseIndex::GetInstance().DeleteReverseIndex(request);
}

ReverseIndexResponse EditReverseIndex(const ReverseIndexRequest& request) {
  return ReverseIndex::GetInstance().EditReverseIndex(request);
}

void ClearTasks() {
  return ReverseIndex::GetInstance().ClearTasks();
}

}  // namespace views::v1::reverse_index