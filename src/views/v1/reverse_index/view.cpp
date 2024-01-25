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

using json = nlohmann::json;

namespace views::v1::reverse_index {

std::string ReverseIndexResponse::ToJSON() const {
  nlohmann::json j;
  j["id"] = id;
  return j.dump();
}

std::string ReverseIndex::AddReverseIndex(
    const ReverseIndexRequest& request) {
  if (!request.cluster || request.fields.empty() || request.id.empty()) {
    return "Invalid arguments";
  }

  auto tasks = tasks_.Lock();
  tasks->push_back(userver::utils::AsyncBackground(
      "ReverseIndexAdd", userver::engine::current_task::GetTaskProcessor(),
      [this, r = std::move(request)]() -> ReverseIndexResponse {
        return this->ReverseIndexFunc(r);
      }));

  return "";
}

ReverseIndex& ReverseIndex::getInstance() {
  static ReverseIndex instance;
  return instance;
}

ReverseIndexResponse ReverseIndex::ReverseIndexFunc(
    ReverseIndexRequest request) const {
  for (std::string& field : request.fields) {
    auto result = request.cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day.reverse_index(key, ids)"
        "VALUES ($1, ARRAY[$2])"
        "ON CONFLICT (key) DO UPDATE SET ids = array_append(reverse_index.ids, "
        "$2);",
        field, request.id);
  }

  ReverseIndexResponse response(request.id);

  return response;
}

}