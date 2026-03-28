#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <userver/components/component_config.hpp>

#include "request_collector.hpp"
#include "lru_request_cache.hpp"
#include "userver/components/component_context.hpp"
#include "userver/logging/log.hpp"
#include "userver/storages/postgres/component.hpp"
#include "request_data.hpp"
#include "userver/yaml_config/merge_schemas.hpp"

namespace middleware::containers {

RequestCollector::RequestCollector(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
    queue_(std::make_unique<LruRequestCache>(
        config["max_lru_size"].As<size_t>(10000),
        config["max_requests_per_user"].As<size_t>(100)
      )),
      pg_cluster_(context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()),
      max_requests_per_user_(config["max_requests_per_user"].As<size_t>(100)) {
    
    LOG_INFO() << "Request Collector started";
}

void RequestCollector::Collect(const userver::server::http::HttpRequest& request,
    const userver::server::request::RequestContext& context) {
    auto try_user_id = context.GetDataOptional<std::string>("user_id");
    if (try_user_id == nullptr) {
        return;
    }
    const std::string& user_id = *try_user_id;

    RequestResponseData data;
    data.url = request.GetUrl();
    data.request_body = request.RequestBody();
    data.timestamp = std::chrono::system_clock::now();

    const auto& response = request.GetHttpResponse();
    data.response_body = response.GetData();
    queue_->Push(user_id, data);

    static const userver::storages::postgres::Query kInsertComment{
        R"(
            INSERT INTO working_day_first.request_cache 
                (user_id, url, request_data, response_data)
            VALUES ($1, $2, $3, $4)
        )",
        userver::storages::postgres::Query::Name{"insert_request"}
    };

    auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            kInsertComment,
            user_id,
            data.url,
            data.request_body,
            data.response_body
    );
    if (result.RowsAffected() == 0) {
      LOG_ERROR() << "No row added for request_cache";
    }
}


std::vector<RequestResponseData> RequestCollector::Get(const std::string& user_id) {
    std::optional<std::vector<RequestResponseData>> ans = queue_->TakeAll(user_id);
    if (ans.has_value()) {
        return ans.value();
    }
    static const userver::storages::postgres::Query kGetLastRequests{
        R"(
            SELECT user_id, url, request_data, response_data, created_at
            FROM working_day_first.request_cache
            WHERE user_id = $1
            ORDER BY created_at DESC
            LIMIT $2
        )",
        userver::storages::postgres::Query::Name{"get_last_requests"}
    };
    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        kGetLastRequests,
        user_id,
        max_requests_per_user_
    );

    return result.AsContainer<std::vector<RequestResponseData>>();
}

RequestCollector::~RequestCollector() {};

userver::yaml_config::Schema RequestCollector::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
        type: object
        description: Request collector with per-user LRU cache
        additionalProperties: false
        properties:
            max_lru_size:
                type: integer
                description: Maximum number of users kept in LRU cache
                defaultDescription: "10000"
            max_requests_per_user:
                type: integer
                description: Maximum number of requests stored per user
                defaultDescription: "200"
    )");
}

};
