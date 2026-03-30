#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <userver/components/component_config.hpp>
#include <userver/formats/parse/common_containers.hpp>
#include <vector>

#include "request_collector.hpp"
#include "lru_request_cache.hpp"
#include "userver/components/component_context.hpp"
#include "userver/formats/json/serialize.hpp"
#include "userver/logging/log.hpp"
#include "userver/storages/postgres/component.hpp"
#include "request_data.hpp"
#include "userver/yaml_config/merge_schemas.hpp"
#include "../MLClient.hpp"

namespace analitics::containers {

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
      ml_client_(context.FindComponent<analitics::ml_client::MlClient>("ml-tcp-client")),
      max_requests_per_user_(config["max_requests_per_user"].As<size_t>(100)) {
    
    LOG_INFO() << "Request Collector started";
}

void RequestCollector::MiddlewareCollect(const userver::server::http::HttpRequest& request,
    const userver::server::request::RequestContext& context) {
    auto try_user_id = context.GetDataOptional<std::string>("user_id");
    if (try_user_id == nullptr) {
        return;
    }
    const std::string& user_id = *try_user_id;
    const auto& response = request.GetHttpResponse();

    std::string request_body_str = request.RequestBody();

    std::string response_body_str = std::string(response.GetData());

    RequestResponseData data {
        .user_id = user_id,
        .url = request.GetUrl(),
        .request_body = request_body_str,
        .response_body = response_body_str,
        .source = "backend",
        .action = "request",
        .action_type = "api_call",
        .created_at    = userver::storages::postgres::TimePointWithoutTz{
                                std::chrono::system_clock::now()}
    };
    PutRequest(data);
}

void RequestCollector::FrontendCollect(const std::string& data) {
    auto j = userver::formats::json::FromString(data);

    RequestResponseData req_data {
        .user_id       = j["user_id"].As<std::string>(""),
        .url           = j["url"].As<std::string>(""),
        .request_body  = j["request_data"].As<std::string>(""),
        .response_body = j["response_data"].As<std::string>(""),
        .source        = j["source"].As<std::string>("frontend"),
        .action        = j["action"].As<std::string>(""),
        .action_type   = j["action_type"].As<std::string>(""),
        .created_at    = userver::storages::postgres::TimePointWithoutTz{
                                std::chrono::system_clock::now()}
        };
    PutRequest(req_data);
}

void RequestCollector::PutRequest(const RequestResponseData& data) {
    queue_->Push(data.user_id, data);

    static const userver::storages::postgres::Query kInsertRequest{
        R"(
            INSERT INTO working_day_first.request_cache 
                (user_id, url, request_data, response_data, created_at, 
                 source, action_type, action)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
        )",
        userver::storages::postgres::Query::Name{"insert_request"}
    };

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        kInsertRequest,
        data.user_id,
        data.url,
        data.request_body,
        data.response_body,
        data.created_at,
        data.source,
        data.action_type,
        data.action
    );

    if (result.RowsAffected() == 0) {
        LOG_ERROR() << "No row added for request_cache";
    }
    
    if (analitics::ml_client::MlClient::ShouldSend(data)) {
        ml_client_.SendToML(Get(data.user_id));
    }
}

namespace details {

std::string SerializeToJson(const std::vector<RequestResponseData>& data)
{
    if (data.empty()) {
        return "[]";
    }
    LOG_ERROR() << "start parsing";
    userver::formats::json::ValueBuilder builder(userver::formats::json::Type::kArray);

    LOG_ERROR() << "build";
    
    for (const auto& item : data) {
        LOG_ERROR() << "call serialize";
        builder.PushBack(item.Serialize());
    }
    
    LOG_ERROR() << "convert to string";
    return userver::formats::json::ToString(builder.ExtractValue());
}

};

std::string RequestCollector::Get(const std::string& user_id) {
    LOG_ERROR() << "=== FIXED Get CALLED for user=" << user_id << " ===";

    // Fast path — LRU cache (this should always succeed after Push)
    std::optional<std::vector<RequestResponseData>> ans = queue_->TakeAll(user_id);
    if (ans.has_value()) {
        LOG_ERROR() << "   → using LRU cache (" << ans->size() << " items)";
        return details::SerializeToJson(ans.value());
    }

    LOG_ERROR() << "   → LRU miss, falling back to DB";

    static const userver::storages::postgres::Query kGetLastRequests{
        R"(
            SELECT 
                user_id, url, request_data, response_data, created_at, 
                source, action_type, action
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

    if (result.IsEmpty()) {
        return "[]";
    }

    std::vector<RequestResponseData> data_vec;
    data_vec.reserve(result.Size());

    LOG_ERROR() << "Point 1";

    for (const auto& row : result) {
        RequestResponseData item;
        item.user_id       = row["user_id"].As<std::string>();
        item.url           = row["url"].As<std::string>();
        item.request_body  = row["request_data"].As<std::string>();
        item.response_body = row["response_data"].As<std::string>();
        item.source        = row["source"].As<std::string>();
        item.action        = row["action"].As<std::string>();
        item.action_type   = row["action_type"].As<std::string>();
        item.created_at    = row["created_at"].As<userver::storages::postgres::TimePointWithoutTz>();
        data_vec.push_back(std::move(item));
    }

    LOG_ERROR() << "Point 2";

    return details::SerializeToJson(data_vec);
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
