#include "queue_manager.hpp"

#include <optional>

namespace core::queue_manager {

void QueueManager::RegisterQueue(const std::string& company_id,
                    const std::string& user_id,
                    std::shared_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>> queue) {
    auto cq = GetCompanyQueues(company_id);
    if (!cq) {
        cq = RegisterCompany(company_id);
    }

    std::lock_guard<std::mutex> lock(cq->mutex);
    cq->queues[user_id] = std::move(queue);
}

void QueueManager::UnregisterQueue(const std::string& company_id, const std::string& user_id) {
    auto cq = GetCompanyQueues(company_id);
    if (!cq) {
        return;
    }

    std::lock_guard<std::mutex> lock(cq->mutex);
    cq->queues.erase(user_id);
}

std::shared_ptr<userver::concurrent::MpscQueue<userver::server::websocket::Message>> QueueManager::GetQueueFor(const std::string& company_id, const std::string& user_id) {
    auto cq = GetCompanyQueues(company_id);
    if (!cq) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(cq->mutex);
    auto it = cq->queues.find(user_id);
    if (it != cq->queues.end()) {
        if (it->second) {
            return it->second;
        } else {
            LOG_WARNING() << "Queue is dead";
            cq->queues.erase(user_id);
        }
    }
    LOG_WARNING() << "Queue not found";
    return nullptr;
}

std::shared_ptr<CompanyQueues> QueueManager::RegisterCompany(const std::string& company_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto cq = std::make_shared<CompanyQueues>();
    companies_[company_id] = std::move(cq);
    return companies_[company_id];
  }

std::shared_ptr<CompanyQueues> QueueManager::GetCompanyQueues(const std::string& company_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (companies_.contains(company_id)) {
        return companies_[company_id];
    }
    return nullptr;
}

}  // namespace core::reverse_index
