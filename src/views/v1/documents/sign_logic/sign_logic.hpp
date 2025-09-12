#pragma once

#include <userver/clients/http/client.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace views::v1::documents::sign::logic {

struct DocumentSignParams {
    std::string company_id;
    std::string user_id;
    std::string document_id;
    userver::clients::http::Client& http_client;
    userver::storages::postgres::ClusterPtr pg_cluster;
    std::string pyservice_url;
    std::string auth_header;
};

std::string SignDocument(const DocumentSignParams& params);

}  // namespace views::v1::documents::sign::logic
