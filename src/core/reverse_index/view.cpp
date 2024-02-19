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
};

void ReverseIndexHandler(const ReverseIndexRequest& request) {
  return ReverseIndex::GetInstance().ReverseIndexHandler(request);
}

void ClearTasks() {
  return ReverseIndex::GetInstance().ClearTasks();
}

}  // namespace views::v1::reverse_index