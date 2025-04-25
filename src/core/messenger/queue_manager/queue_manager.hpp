#pragma once

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/concurrent/mpsc_queue.hpp>
#include <userver/server/websocket/websocket_handler.hpp>

#include <mutex>
#include <unordered_map>
#include <string>
#include <memory>

namespace core::queue_manager {

struct CompanyQueues {
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>>> queues;
};

class QueueManager {
 public:
  void RegisterQueue(const std::string& company_id,
                     const std::string& user_id,
                     std::shared_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>> queue);
  void UnregisterQueue(const std::string& company_id, const std::string& user_id);

  std::shared_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>> GetQueueFor(const std::string& company_id, const std::string& user_id);

  static QueueManager& GetInstance() {
    static QueueManager instance;
    return instance;
  }

 private:
  QueueManager() = default;
  ~QueueManager() = default;

  QueueManager(const QueueManager&) = delete;
  QueueManager& operator=(const QueueManager&) = delete;

  std::shared_ptr<CompanyQueues> RegisterCompany(const std::string& company_id);

  std::shared_ptr<CompanyQueues> GetCompanyQueues(const std::string& company_id);

  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<CompanyQueues>> companies_;
};

}  // namespace core::reverse_index