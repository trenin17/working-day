#pragma once

#include <core/json_compatible/struct.hpp>

#ifdef V1_EMPLOYEES
  #define USE_EMPLOYEES_RESPONSE
#endif

#ifdef USE_EMPLOYEES_RESPONSE
  #define USE_LIST_EMPLOYEE
#endif

#ifdef V1_SEARCH_BASIC
  #define USE_SEARCH_BASIC_REQUEST
  #define USE_SEARCH_BASIC_RESPONSE
#endif

#ifdef USE_SEARCH_BASIC_RESPONSE
  #define USE_LIST_EMPLOYEE
#endif

#ifdef USE_LIST_EMPLOYEE
struct ListEmployee: public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  ListEmployee() = default;

  // Make sure to initialize parsing first for new structure
  ListEmployee(ListEmployee&& other) {
    *this = std::move(other);
  }

  ListEmployee& operator=(ListEmployee&& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(id, name, surname, patronymic, photo_link);
  }

  REGISTER_STRUCT_FIELD(id, std::string, "id");
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD(surname, std::string, "surname");
  REGISTER_STRUCT_FIELD_OPTIONAL(patronymic, std::string, "patronymic");
  REGISTER_STRUCT_FIELD_OPTIONAL(photo_link, std::string, "photo_link");
};
#endif

#ifdef USE_SEARCH_BASIC_REQUEST
struct SearchBasicRequest: public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
};
#endif

#ifdef USE_SEARCH_BASIC_RESPONSE
struct SearchBasicResponse: public JsonCompatible {
  REGISTER_STRUCT_FIELD(employees, std::vector<ListEmployee>, "employees");
};
#endif

#ifdef USE_EMPLOYEES_RESPONSE
struct EmployeesResponse: public JsonCompatible {
  REGISTER_STRUCT_FIELD(employees, std::vector<ListEmployee>, "employees");
};
#endif
