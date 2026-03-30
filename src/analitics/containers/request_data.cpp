#include "request_data.hpp"

#include <userver/formats/json/value_builder.hpp>
#include <userver/utils/datetime.hpp>
#include "userver/formats/json/inline.hpp"

namespace analitics::containers {

userver::formats::json::Value RequestResponseData::Serialize() const {
    return userver::formats::json::MakeObject(
        "user_id",       user_id,
        "url",           url,
        "request_body",  request_body,
        "response_body", response_body,
        "source",        source,
        "action",        action,
        "action_type",   action_type,
        "created_at",    userver::utils::datetime::Timestring(created_at)
    );
}

} // namespace analitics::containers

namespace userver::formats::parse {

analitics::containers::RequestResponseData Parse(
    const userver::formats::json::Value& value,
    userver::formats::parse::To<analitics::containers::RequestResponseData>) {

    analitics::containers::RequestResponseData data;

    data.user_id       = value["user_id"].As<std::string>("");
    data.url           = value["url"].As<std::string>("");
    data.request_body  = value["request_body"].As<std::string>("");
    data.response_body = value["response_body"].As<std::string>("");
    data.source        = value["source"].As<std::string>("");
    data.action        = value["action"].As<std::string>("");
    data.action_type   = value["action_type"].As<std::string>("");

    if (value.HasMember("created_at") && !value["created_at"].IsNull()) {
        auto tp = userver::utils::datetime::Stringtime(
            value["created_at"].As<std::string>());
        data.created_at = userver::storages::postgres::TimePointWithoutTz{tp};
    } else {
        data.created_at = userver::storages::postgres::TimePointWithoutTz{
            std::chrono::system_clock::now()};
    }

    return data;
}

} // namespace userver::formats::parse