#pragma once

#include <cstdint>
#include <memory>
#include <userver/components/component_base.hpp>
#include <userver/concurrent/queue.hpp>
#include <userver/engine/task/task.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "lru_request_cache.hpp"
#include "request_data.hpp"
#include "userver/server/http/http_request.hpp"
#include "userver/server/request/request_context.hpp"

namespace middleware::containers {

class RequestCollector final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "request-collector";

    RequestCollector(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context);
    
    ~RequestCollector() override;

    void Collect(const userver::server::http::HttpRequest& request,
                 const userver::server::request::RequestContext& context);

    std::vector<RequestResponseData> Get(const std::string& user_id);
    
    static userver::yaml_config::Schema GetStaticConfigSchema();
private:
    std::unique_ptr<LruRequestCache> queue_;
    userver::storages::postgres::ClusterPtr pg_cluster_;
    const int64_t max_requests_per_user_;
// int64_t instead of size_t cause Postgress does not give a fuck what is size_t lol
};

}  // namespace middleware