#pragma once

#include <core/json_compatible/struct.hpp>

#ifdef V1_EMPLOYEES
#define USE_EMPLOYEES_RESPONSE
#endif

#ifdef USE_EMPLOYEES_RESPONSE
#define USE_LIST_EMPLOYEE
#endif

#ifdef V1_ADD_EMPLOYEE
#define USE_ADD_EMPLOYEE_REQUEST
#define USE_ADD_EMPLOYEE_RESPONSE
#define USE_REVERSE_INDEX
#endif

#ifdef V1_REMOVE_EMPLOYEE
#define USE_ERROR_MESSAGE
#define USE_REVERSE_INDEX
#endif

#ifdef V1_EDIT_EMPLOYEE
#define USE_PROFILE_EDIT_REQUEST
#define USE_REVERSE_INDEX
#endif

#ifdef V1_SEARCH_BASIC
#define USE_SEARCH_BASIC_REQUEST
#define USE_SEARCH_RESPONSE
#endif

#ifdef V1_SEARCH_FULL
#define USE_SEARCH_FULL_REQUEST
#define USE_SEARCH_RESPONSE
#endif

#ifdef V1_SEARCH_SUGGEST
#define USE_SEARCH_SUGGEST_REQUEST
#define USE_SEARCH_RESPONSE
#endif

#ifdef USE_SEARCH_RESPONSE
#define USE_LIST_EMPLOYEE
#endif

#ifdef V1_ATTENDANCE_LIST_ALL
#define USE_ATTENDANCE_LIST_ALL_REQUEST
#define USE_ATTENDANCE_LIST_ALL_RESPONSE
#endif

#ifdef USE_ATTENDANCE_LIST_ALL_RESPONSE
#define USE_ATTENDANCE_LIST_ITEM
#endif

#ifdef USE_ATTENDANCE_LIST_ITEM
#define USE_LIST_EMPLOYEE
#endif

#ifdef V1_DOCUMENTS_UPLOAD
#define USE_UPLOAD_DOCUMENT_RESPONSE
#endif

#ifdef V1_DOCUMENTS_SEND
#define USE_DOCUMENT_SEND_REQUEST
#endif

#ifdef USE_DOCUMENT_SEND_REQUEST
#define USE_DOCUMENT_ITEM
#endif

#ifdef V1_DOCUMENTS_LIST
#define USE_DOCUMENTS_LIST_RESPONSE
#endif

#ifdef USE_DOCUMENTS_LIST_RESPONSE
#define USE_DOCUMENT_ITEM
#endif

#ifdef V1_DOCUMENTS_DOWNLOAD
#define USE_DOWNLOAD_DOCUMENT_RESPONSE
#endif

#ifdef USE_LIST_EMPLOYEE
struct ListEmployee : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  ListEmployee() = default;

  // Make sure to initialize parsing first for new structure
  ListEmployee(ListEmployee&& other) { *this = std::move(other); }

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

#ifdef USE_ADD_EMPLOYEE_REQUEST
struct AddEmployeeRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD(surname, std::string, "surname");
  REGISTER_STRUCT_FIELD(role, std::string, "role");
  REGISTER_STRUCT_FIELD_OPTIONAL(patronymic, std::string, "patronymic");
};
#endif

#ifdef USE_ADD_EMPLOYEE_RESPONSE
struct AddEmployeeResponse : public JsonCompatible {
  AddEmployeeResponse(const std::string& l, const std::string& p) {
    login = l;
    password = p;
  }

  REGISTER_STRUCT_FIELD(login, std::string, "login");
  REGISTER_STRUCT_FIELD(password, std::string, "password");
};
#endif

#ifdef USE_ERROR_MESSAGE
struct ErrorMessage : public JsonCompatible {
  ErrorMessage(const std::string& msg) { message = msg; }

  REGISTER_STRUCT_FIELD(message, std::string, "message");
};
#endif

#ifdef USE_PROFILE_EDIT_REQUEST
struct ProfileEditRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD_OPTIONAL(phones, std::vector<std::string>, "phones");
  REGISTER_STRUCT_FIELD_OPTIONAL(email, std::string, "email");
  REGISTER_STRUCT_FIELD_OPTIONAL(birthday, std::string, "birthday");
  REGISTER_STRUCT_FIELD_OPTIONAL(password, std::string, "password");
  REGISTER_STRUCT_FIELD_OPTIONAL(telegram_id, std::string, "telegram_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(vk_id, std::string, "vk_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(team, std::string, "team");
};
#endif

#ifdef USE_SEARCH_BASIC_REQUEST
struct SearchBasicRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
};
#endif

#ifdef USE_SEARCH_FULL_REQUEST
struct SearchFullRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
  REGISTER_STRUCT_FIELD(limit, int, "limit");
};
#endif

#ifdef USE_SEARCH_SUGGEST_REQUEST
struct SearchSuggestRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
  REGISTER_STRUCT_FIELD(limit, int, "limit");
};
#endif

#ifdef USE_SEARCH_RESPONSE
struct SearchResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(employees, std::vector<ListEmployee>, "employees");
};
#endif

#ifdef USE_EMPLOYEES_RESPONSE
struct EmployeesResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(employees, std::vector<ListEmployee>, "employees");
};
#endif

#ifdef USE_ATTENDANCE_LIST_ALL_REQUEST
struct AttendanceListAllRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(from, userver::storages::postgres::TimePoint, "from");
  REGISTER_STRUCT_FIELD(to, userver::storages::postgres::TimePoint, "to");
};
#endif

#ifdef USE_ATTENDANCE_LIST_ITEM
struct AttendanceListItem : public JsonCompatible {
  AttendanceListItem() = default;

  AttendanceListItem(AttendanceListItem&& other) { *this = std::move(other); }

  AttendanceListItem& operator=(AttendanceListItem&& other) = default;

  auto Introspect() { return std::tie(start_date, end_date, employee); }

  REGISTER_STRUCT_FIELD_OPTIONAL(start_date,
                                 userver::storages::postgres::TimePoint,
                                 "start_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(end_date,
                                 userver::storages::postgres::TimePoint,
                                 "end_date");
  REGISTER_STRUCT_FIELD(employee, ListEmployee, "employee");
  // REGISTER_STRUCT_FIELD_OPTIONAL(abscence_date,
  // userver::storages::postgres::TimePoint, "abscence_date");
};
#endif

#ifdef USE_ATTENDANCE_LIST_ALL_RESPONSE
struct AttendanceListAllResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(attendances, std::vector<AttendanceListItem>,
                        "attendances");
};
#endif

#ifdef USE_UPLOAD_DOCUMENT_RESPONSE
struct UploadDocumentResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(url, std::string, "url");
  REGISTER_STRUCT_FIELD(id, std::string, "id");
};
#endif

#ifdef USE_DOCUMENT_ITEM
struct DocumentItem : public JsonCompatible {
  DocumentItem() = default;

  DocumentItem(DocumentItem&& other) { *this = std::move(other); }

  DocumentItem& operator=(DocumentItem&& other) = default;

  auto Introspect() { return std::tie(id, name, sign_required, description); }

  REGISTER_STRUCT_FIELD(id, std::string, "id");
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD(sign_required, bool, "sign_required", false);
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
};
#endif

#ifdef USE_DOCUMENT_SEND_REQUEST
struct DocumentSendRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(document, DocumentItem, "document");
  REGISTER_STRUCT_FIELD(employee_ids, std::vector<std::string>, "employee_ids");
};
#endif

#ifdef USE_DOCUMENTS_LIST_RESPONSE
struct DocumentsListResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(documents, std::vector<DocumentItem>, "documents");
};
#endif

#ifdef USE_DOWNLOAD_DOCUMENT_RESPONSE
struct DownloadDocumentResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(url, std::string, "url");
};
#endif
