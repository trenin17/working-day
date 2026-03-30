#pragma once

#include <string>
#include <chrono>

#include <userver/formats/json/value.hpp>
#include <userver/formats/parse/to.hpp>
#include <userver/storages/postgres/io/chrono.hpp>

namespace analitics::containers {

struct RequestResponseData {
    std::string user_id;
    std::string url;
    std::string request_body;
    std::string response_body;
    std::string source;
    std::string action;
    std::string action_type;

    userver::storages::postgres::TimePointWithoutTz created_at{
        std::chrono::system_clock::now()};

    userver::formats::json::Value Serialize() const;
};

}  // namespace analitics::containers

namespace userver::formats::parse {

analitics::containers::RequestResponseData Parse(
    const userver::formats::json::Value& value,
    userver::formats::parse::To<analitics::containers::RequestResponseData>);

}  // namespace userver::formats::parse