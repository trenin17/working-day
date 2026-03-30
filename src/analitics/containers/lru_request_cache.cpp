#include "lru_request_cache.hpp"
#include <cstddef>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace analitics::containers {

LruRequestCache::LruRequestCache( size_t max_lru_size, size_t max_requests_per_user)
    : max_lru_size_(max_lru_size)
    , max_requests_per_user_(max_requests_per_user)
    , cache_(max_lru_size)
{
}

void LruRequestCache::Push(const std::string& user_id, const RequestResponseData& request) {
    std::unique_lock _(mutex_);
    std::deque<RequestResponseData>* queue_ptr = cache_.Emplace(user_id);

    if (queue_ptr == nullptr) {
        queue_ptr = cache_.Get(user_id);
    }

    auto& queue = *queue_ptr;

    queue.push_back(std::move(request));

    while (queue.size() > max_requests_per_user_) {
        queue.pop_front();
    }
}

std::optional<std::vector<RequestResponseData>> LruRequestCache::TakeAll(const std::string& user_id) {
    std::shared_lock _(mutex_);
    
    if (const auto* queue_ptr = cache_.Get(user_id)) {
        return std::vector<RequestResponseData>(queue_ptr->begin(), queue_ptr->end());
    }
    return std::nullopt;
}

size_t LruRequestCache::MaxRequestsPerUser() const {
    return max_requests_per_user_;
}

};