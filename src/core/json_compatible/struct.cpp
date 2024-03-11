#include "struct.hpp"

#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

void JsonCompatible::ParseRegisteredFields(const std::string& body) {
  auto json = nlohmann::json::parse(body);
  for (auto& lambda : lambdas_parse_) {
    lambda(json);
  }
}

nlohmann::json JsonCompatible::ToJson() const {
  nlohmann::json json;
  for (auto& lambda : lambdas_dump_) {
    lambda(json);
  }

  return json;
}

std::string JsonCompatible::ToJsonString() const { return ToJson().dump(); }
