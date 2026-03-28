#pragma once

#include <string>
// #include <unordered_map>
#include <chrono>

namespace middleware::containers {

struct RequestResponseData {
    std::string user_id;
    std::string url;
    std::string request_body;
    std::string response_body;
    std::chrono::system_clock::time_point timestamp;
    // std::unordered_map<std::string, std::string> request_headers;
    // std::unordered_map<std::string, std::string> response_headers;  // can't get it ;(
};

}  // namespace middleware