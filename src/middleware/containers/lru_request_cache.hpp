#pragma once

#include <cstddef>
#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>
#include <userver/concurrent/variable.hpp>
#include <userver/cache/lru_map.hpp>

#include <deque>
#include <string>
#include <vector>
#include "request_data.hpp"
#include "userver/engine/shared_mutex.hpp"

namespace middleware::containers {

class LruRequestCache {
public:
    LruRequestCache(size_t max_lru_size,
                    size_t max_requests_per_user);

    void Push(const std::string& user_id, const RequestResponseData& request);
    std::optional<std::vector<RequestResponseData>> TakeAll(const std::string& user_id);

    size_t MaxRequestsPerUser() const;

private:
    const size_t max_lru_size_;
    const size_t max_requests_per_user_;

    userver::engine::SharedMutex mutex_;
// im actually dont wanna to add raw mutex inside my code, but that userver version have no better interface 
    userver::cache::LruMap<std::string, std::deque<RequestResponseData>> cache_;
};

};