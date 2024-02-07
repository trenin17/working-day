#include <nlohmann/json_fwd.hpp>
#include <functional>

class JsonCompatible {
public:
    void ParseRegisteredFields(const std::string& body);

    template <class T>
    void RegisterParsing(T* /*ptr*/, std::string /*parameter_name*/, bool mandatory);

    template <class T>
    void RegisterDumping(T* /*ptr*/, std::string /*parameter_name*/, bool mandatory);

    std::string ToJsonString();

protected:
    std::vector<std::function<void(nlohmann::json&)>> lambdas_parse_;
    std::vector<std::function<void(nlohmann::json&)>> lambdas_dump_;
};

#define REGISTER_STRUCT_FIELD_INTERNAL_DEFAULT_VALUE(variable_name, type, json_key, default_value) \
    type variable_name = [this, variable_name_addr = &(variable_name)]() -> type {                 \
        RegisterParsing(variable_name_addr, json_key, false);                                      \
        RegisterDumping(variable_name_addr, json_key, false);                                      \
        return default_value;                                                                      \
    }()

#define REGISTER_STRUCT_FIELD_INTERNAL_MANDATORY_VALUE(variable_name, type, json_key) \
    type variable_name = [this, variable_name_addr = &(variable_name)]() -> type {    \
        RegisterParsing(variable_name_addr, json_key, true);                          \
        RegisterDumping(variable_name_addr, json_key, true);                          \
        return type();                                                                \
    }()

#define REGISTER_STRUCT_FIELD_INTERMEDIATE(x, A, B, C, D, FUNC, ...) FUNC

// The macro that the programmer uses for registering field
#define REGISTER_STRUCT_FIELD(...)                                                                \
    REGISTER_STRUCT_FIELD_INTERMEDIATE(,                                                          \
                                       ##__VA_ARGS__,                                             \
                                       REGISTER_STRUCT_FIELD_INTERNAL_DEFAULT_VALUE(__VA_ARGS__), \
                                       REGISTER_STRUCT_FIELD_INTERNAL_MANDATORY_VALUE(__VA_ARGS__))

// The macro that the programmer uses for registering optional field
#define REGISTER_STRUCT_FIELD_OPTIONAL(variable_name, type, json_key)                                      \
    REGISTER_STRUCT_FIELD_INTERNAL_DEFAULT_VALUE(variable_name, std::optional<type>, json_key, std::nullopt)
