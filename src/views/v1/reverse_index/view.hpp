#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

namespace views::v1::reverse_index {

class ReverseIndexResponse {
    public:
        ReverseIndexResponse(const std::string& id) : id(id) {}

        std::string ToJSON() const;

        std::string id;
};

class ReverseIndexRequest {
public:
    ReverseIndexRequest(userver::storages::postgres::ClusterPtr cluster, std::string& id, std::vector<std::string>& fields) 
                        : cluster(cluster), id(id), fields(fields) {}

    userver::storages::postgres::ClusterPtr cluster;
    std::string id;
    std::vector<std::string> fields;
};

class ReverseIndex{
public:
    std::string AddReverseIndex(const ReverseIndexRequest& request);

    static ReverseIndex& getInstance();
private:
    ReverseIndex() = default;
    ~ReverseIndex() = default;

    ReverseIndex(const ReverseIndex&) = delete;
    ReverseIndex& operator=(const ReverseIndex&) = delete;

    ReverseIndexResponse ReverseIndexFunc(ReverseIndexRequest request) const;

    mutable userver::concurrent::Variable<std::vector<userver::engine::TaskWithResult<ReverseIndexResponse>>> tasks_;
};
    
}