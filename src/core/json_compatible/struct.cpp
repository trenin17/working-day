#include "struct.hpp"

#include <optional>
#include <vector>

#include <userver/logging/log.hpp>

#include <nlohmann/json.hpp>

namespace detail {

template <class T>
void Parse(T* /*res*/, const nlohmann::json& /*json*/) {
    throw std::runtime_error("Please provide parse specialization for requested type");
}

template <>
void Parse(bool* res, const nlohmann::json& json) {
    *res = json.get<bool>();
}

template <>
void Parse(std::string* res, const nlohmann::json& json) {
    *res = json.get<std::string>();
}

template <>
void Parse(double* res, const nlohmann::json& json) {
    *res = json.get<double>();
}

template <>
void Parse(int64_t* res, const nlohmann::json& json) {
    *res = json.get<int64_t>();
}

template <>
void Parse(int* res, const nlohmann::json& json) {
    *res = json.get<int>();
}

template <>
void Parse(uint64_t* res, const nlohmann::json& json) {
    *res = json.get<uint64_t>();
}

template <>
void Parse(uint32_t* res, const nlohmann::json& json) {
    *res = json.get<uint32_t>();
}

template <>
void Parse(std::vector<std::string>* res, const nlohmann::json& json) {
    *res = json.get<std::vector<std::string>>();
}

template <class T>
void Parse(std::optional<T>* res, const nlohmann::json& json) {
    T temp;
    Parse(&temp, json);
    *res = std::move(temp);
}


template <class T>
void Dump(const T& /*field*/,  const std::string& /* parameter_name */, nlohmann::json& /*json*/) {
    throw std::runtime_error("Please provide dump specialization for requested type");
}

template <>
void Dump(const std::string& field, const std::string& parameter_name, nlohmann::json& json) {
    json[parameter_name] = field;
}

template <>
void Dump(const std::vector<std::string>& field, const std::string& parameter_name, nlohmann::json& json) {
    json[parameter_name] = field;
}

} // namespace detail

template<typename T>
struct is_std_optional : std::false_type {};

// Специализация для std::optional
template<typename T>
struct is_std_optional<std::optional<T>> : std::true_type {};

// Вспомогательный шаблон для удобства
template<typename T>
inline constexpr bool is_std_optional_v = is_std_optional<T>::value;

template <class T>
void JsonCompatible::RegisterParsing(T* ptr, std::string parameter_name, bool mandatory) {
    lambdas_parse_.push_back([ptr, parameter_name = std::move(parameter_name), mandatory](nlohmann::json& json) {
        if (json.contains(parameter_name)) {
            detail::Parse(ptr, json.at(parameter_name));
            json.erase(parameter_name);
        } else if (mandatory) {
            throw std::runtime_error("No parameter " + parameter_name + " found in struct");
        }
    });
}

void JsonCompatible::ParseRegisteredFields(const std::string& body) {
    auto json = nlohmann::json::parse(body);
    for (auto& lambda : lambdas_parse_) {
        lambda(json);
    }
}

template <class T>
void JsonCompatible::RegisterDumping(T* ptr, std::string parameter_name, bool mandatory) {
    lambdas_dump_.push_back([ptr, parameter_name = std::move(parameter_name), mandatory](nlohmann::json& json) {
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

std::string JsonCompatible::ToJsonString() {
    nlohmann::json json;
    for (auto& lambda : lambdas_dump_) {
        lambda(json);
    }

    return json.dump();
}

// template void
// JsonCompatible::RegisterParsing(bool* ptr, std::string, bool);

// template void
// JsonCompatible::RegisterParsing(int* ptr, std::string, bool);

// template void
// JsonCompatible::RegisterParsing(int64_t* ptr, std::string, bool);

// template void
// JsonCompatible::RegisterParsing(uint64_t* ptr, std::string, bool);

// template void
// JsonCompatible::RegisterParsing(uint32_t* ptr, std::string, bool);

// template void
// JsonCompatible::RegisterParsing(double* ptr, std::string, bool);

template void JsonCompatible::RegisterParsing(std::string* ptr, std::string, bool);

template void JsonCompatible::RegisterParsing(std::vector<std::string>* ptr, std::string, bool);

template void JsonCompatible::RegisterParsing(std::optional<std::string>* ptr, std::string, bool);

template void JsonCompatible::RegisterParsing(std::optional<std::vector<std::string>>* ptr, std::string, bool);


template void JsonCompatible::RegisterDumping(std::string* ptr, std::string, bool);

template void JsonCompatible::RegisterDumping(std::vector<std::string>* ptr, std::string, bool);

template void JsonCompatible::RegisterDumping(std::optional<std::string>* ptr, std::string, bool);

template void JsonCompatible::RegisterDumping(std::optional<std::vector<std::string>>* ptr, std::string, bool);
