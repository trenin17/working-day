#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/utils/datetime.hpp>

#include <nlohmann/json.hpp>

class JsonCompatible {
 public:
  JsonCompatible() = default;

  JsonCompatible(const JsonCompatible&) { JsonCompatible(); }

  JsonCompatible& operator=(const JsonCompatible&) { return *this; }

  JsonCompatible(JsonCompatible&&) { JsonCompatible(); }

  JsonCompatible& operator=(JsonCompatible&&) { return *this; }

  void ParseRegisteredFields(const std::string& body);

  template <class T>
  void RegisterParsing(T* ptr, std::string parameter_name, bool mandatory);

  template <class T>
  void RegisterDumping(T* ptr, std::string parameter_name, bool mandatory);

  nlohmann::json ToJson() const;

  std::string ToJsonString() const;

 private:
  std::vector<std::function<void(nlohmann::json&)>> lambdas_parse_;
  std::vector<std::function<void(nlohmann::json&)>> lambdas_dump_;

  template <typename T>
  struct is_std_optional : std::false_type {};

  template <typename T>
  struct is_std_optional<std::optional<T>> : std::true_type {};

  template <typename T>
  inline static constexpr bool is_std_optional_v = is_std_optional<T>::value;
};

namespace detail {
template <class T>
inline void Parse(T* /*res*/, const nlohmann::json& /*json*/) {
  throw std::runtime_error(
      "Please provide parse specialization for requested type");
}

template <class T>
inline void Dump(const T& /*field*/, const std::string& /*parameter_name*/,
                 nlohmann::json& /*json*/) {
  throw std::runtime_error(
      "Please provide dump specialization for requested type: " +
      std::string(typeid(T).name()));
}

template <>
inline void Parse(bool* res, const nlohmann::json& json) {
  *res = json.get<bool>();
}

template <>
inline void Parse(std::string* res, const nlohmann::json& json) {
  *res = json.get<std::string>();
}

template <>
inline void Parse(double* res, const nlohmann::json& json) {
  *res = json.get<double>();
}

template <>
inline void Parse(int64_t* res, const nlohmann::json& json) {
  *res = json.get<int64_t>();
}

template <>
inline void Parse(int* res, const nlohmann::json& json) {
  *res = json.get<int>();
}

template <>
inline void Parse(uint64_t* res, const nlohmann::json& json) {
  *res = json.get<uint64_t>();
}

template <>
inline void Parse(uint32_t* res, const nlohmann::json& json) {
  *res = json.get<uint32_t>();
}

template <>
inline void Parse(std::vector<std::string>* res, const nlohmann::json& json) {
  *res = json.get<std::vector<std::string>>();
}

template <>
inline void Parse(userver::storages::postgres::TimePoint* res,
                  const nlohmann::json& json) {
  *res = userver::utils::datetime::Stringtime(json.get<std::string>(), "UTC",
                                              "%Y-%m-%dT%H:%M:%E6S");
  ;
}

template <typename T>
concept DerivedFromJsonCompatible = std::is_base_of<JsonCompatible, T>::value;

template <DerivedFromJsonCompatible T>
inline void Parse(T* res, const nlohmann::json& json) {
  res->ParseRegisteredFields(json.dump());
}

template <DerivedFromJsonCompatible T>
inline void Parse(std::vector<T>* res, const nlohmann::json& json) {
  for (const auto& item : json) {
    res->emplace_back().ParseRegisteredFields(item.dump());
  }
}

template <class T>
inline void Parse(std::optional<T>* res, const nlohmann::json& json) {
  T temp;
  Parse(&temp, json);
  *res = std::move(temp);
}

template <>
inline void Dump(const std::string& field, const std::string& parameter_name,
                 nlohmann::json& json) {
  json[parameter_name] = field;
}

template <>
inline void Dump(const bool& field, const std::string& parameter_name,
                 nlohmann::json& json) {
  json[parameter_name] = field;
}

template <>
inline void Dump(const std::vector<std::string>& field,
                 const std::string& parameter_name, nlohmann::json& json) {
  json[parameter_name] = field;
}

template <>
inline void Dump(const int& field, const std::string& parameter_name,
                 nlohmann::json& json) {
  json[parameter_name] = field;
}

template <DerivedFromJsonCompatible T>
inline void Dump(const T& field, const std::string& parameter_name,
                 nlohmann::json& json) {
  json[parameter_name] = field.ToJson();
}

template <>
inline void Dump(const userver::storages::postgres::TimePoint& field,
                 const std::string& parameter_name, nlohmann::json& json) {
  json[parameter_name] =
      userver::utils::datetime::Timestring(field, "UTC", "%Y-%m-%dT%H:%M:%E6S");
}

template <DerivedFromJsonCompatible T>
inline void Dump(const std::vector<T>& field, const std::string& parameter_name,
                 nlohmann::json& json) {
  json[parameter_name] = nlohmann::json::array();
  for (const auto& item : field) {
    json[parameter_name].push_back(item.ToJson());
  }
}

}  // namespace detail

template <class T>
void JsonCompatible::RegisterParsing(T* ptr, std::string parameter_name,
                                     bool mandatory) {
  lambdas_parse_.push_back([ptr, parameter_name = std::move(parameter_name),
                            mandatory](nlohmann::json& json) {
    if (json.contains(parameter_name)) {
      detail::Parse(ptr, json.at(parameter_name));
      json.erase(parameter_name);
    } else if (mandatory) {
      throw std::runtime_error("No parameter " + parameter_name +
                               " found in struct");
    }
  });
}

template <class T>
void JsonCompatible::RegisterDumping(T* ptr, std::string parameter_name,
                                     bool mandatory) {
  lambdas_dump_.push_back([ptr, parameter_name = std::move(parameter_name),
                           mandatory](nlohmann::json& json) {
    if constexpr (is_std_optional_v<T>) {
      if (*ptr == std::nullopt) {
        if (mandatory) {
          throw std::runtime_error("Missing mandatory field " + parameter_name);
        }
      } else {
        detail::Dump(ptr->value(), parameter_name, json);
      }
    } else {
      detail::Dump(*ptr, parameter_name, json);
    }
  });
}

#define REGISTER_STRUCT_FIELD_INTERNAL_DEFAULT_VALUE(variable_name, type,     \
                                                     json_key, default_value) \
  type variable_name = [this,                                                 \
                        variable_name_addr = &(variable_name)]() -> type {    \
    RegisterParsing(variable_name_addr, json_key, false);                     \
    RegisterDumping(variable_name_addr, json_key, false);                     \
    return default_value;                                                     \
  }()

#define REGISTER_STRUCT_FIELD_INTERNAL_MANDATORY_VALUE(variable_name, type, \
                                                       json_key)            \
  type variable_name = [this,                                               \
                        variable_name_addr = &(variable_name)]() -> type {  \
    RegisterParsing(variable_name_addr, json_key, true);                    \
    RegisterDumping(variable_name_addr, json_key, true);                    \
    return type();                                                          \
  }()

#define REGISTER_STRUCT_FIELD_INTERMEDIATE(x, A, B, C, D, FUNC, ...) FUNC

// The macro that the programmer uses for registering field
#define REGISTER_STRUCT_FIELD(...)                               \
  REGISTER_STRUCT_FIELD_INTERMEDIATE(                            \
      , ##__VA_ARGS__,                                           \
      REGISTER_STRUCT_FIELD_INTERNAL_DEFAULT_VALUE(__VA_ARGS__), \
      REGISTER_STRUCT_FIELD_INTERNAL_MANDATORY_VALUE(__VA_ARGS__))

// The macro that the programmer uses for registering optional field
#define REGISTER_STRUCT_FIELD_OPTIONAL(variable_name, type, json_key) \
  REGISTER_STRUCT_FIELD_INTERNAL_DEFAULT_VALUE(                       \
      variable_name, std::optional<type>, json_key, std::nullopt)
