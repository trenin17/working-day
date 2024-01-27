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

std::string ReverseIndexResponse::ToJSON() const {
  nlohmann::json j;
  j["id"] = employee_id;
  return j.dump();
}

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
          return this->ReverseIndexFunc(r);
        }));
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

  ReverseIndexResponse ReverseIndexFunc(
      const ReverseIndexRequest& request) const {
    for (auto& field : request.fields) {
      if (!field.has_value()) continue;
      auto result = request.cluster->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "INSERT INTO working_day.reverse_index(key, ids)"
          "VALUES ($1, ARRAY[$2])"
          "ON CONFLICT (key) DO UPDATE SET ids = "
          "array_append(reverse_index.ids, "
          "$2);",
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

}  // namespace views::v1::reverse_index