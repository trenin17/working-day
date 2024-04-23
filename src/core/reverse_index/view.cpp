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

namespace core::reverse_index {

std::string ConvertToLower(std::string s) {
  std::setlocale(LC_ALL, "en_US.UTF-8");
  std::wstring wstr =
      std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(s);
  ;
  for (size_t i = 0; i < wstr.size(); ++i) {
    wchar_t lower = std::towlower(wstr[i]);
    wstr[i] = lower;
  }
  using convert_type = std::codecvt_utf8<wchar_t>;
  std::wstring_convert<convert_type, wchar_t> converter;
  std::string converted_str = converter.to_bytes(wstr);
  return converted_str;
}

class ReverseIndex {
 public:
  void ReverseIndexHandler(const ReverseIndexRequest& request) {
    auto tasks = tasks_.Lock();
    while (!tasks->empty() && tasks->front().IsFinished()) {
      tasks->pop();
    }

    tasks->push(userver::utils::AsyncBackground(
        "ReverseIndexFunc", userver::engine::current_task::GetTaskProcessor(),
        request.func));
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

void ClearTasks() { return ReverseIndex::GetInstance().ClearTasks(); }

}  // namespace core::reverse_index