#pragma once

#include <core/json_compatible/struct.hpp>
#include <userver/storages/postgres/io/io_fwd.hpp>

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
#define USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
#endif

#ifdef V1_DOCUMENTS_UPLOAD
#define USE_UPLOAD_DOCUMENT_REQUEST
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

#ifdef V1_DOCUMENTS_LIST_ALL
#define USE_DOCUMENTS_LIST_ALL_RESPONSE
#endif

#ifdef USE_DOCUMENTS_LIST_ALL_RESPONSE
#define USE_DOCUMENT_ITEM
#endif

#ifdef V1_DOCUMENTS_GET_SIGNS
#define USE_DOCUMENTS_GET_SIGNS_RESPONSE
#endif

#ifdef USE_DOCUMENTS_GET_SIGNS_RESPONSE
#define USE_SIGN_ITEM
#endif

#ifdef USE_SIGN_ITEM
#define USE_LIST_EMPLOYEE
#endif

#ifdef V1_ACTIONS
#define USE_ACTIONS_REQUEST
#define USE_ACTIONS_RESPONSE
#endif

#ifdef USE_ACTIONS_RESPONSE
#define USE_USER_ACTION
#endif

#ifdef V1_SUPERUSER_COMPANY_ADD
#define USE_SUPERUSER_COMPANY_ADD_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_AUTHORIZE
#define USE_AUTHORIZE_REQUEST
#define USE_AUTHORIZE_RESPONSE
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_DOCUMENTS_VACATION
#define USE_PYSERVICE_DOCUMENT_GENERATE_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_ABSCENCE_VERDICT
#define USE_PYSERVICE_DOCUMENT_GENERATE_REQUEST
#define USE_ABSCENCE_VERDICT_REQUEST
#endif

#ifdef V1_DOCUMENTS_SIGN
#define USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
#define USE_PYSERVICE_DOCUMENT_SIGN_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
#define USE_LIST_EMPLOYEE
#endif

#ifdef V1_ABSCENCE_REQUEST
#define USE_ABSCENCE_REQUEST_REQUEST
#define USE_ABSCENCE_REQUEST_RESPONSE
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_INVENTORY_ADD
#define USE_INVENTORY_ADD_REQUEST
#endif

#ifdef USE_INVENTORY_ADD_REQUEST
#define USE_INVENTORY_ITEM
#endif

#ifdef V1_EMPLOYEE_INFO
#define USE_EMPLOYEE
#define USE_ERROR_MESSAGE
#endif

#ifdef USE_EMPLOYEE
#define USE_LIST_EMPLOYEE
#define USE_INVENTORY_ITEM
#endif

#ifdef USE_LIST_EMPLOYEE
struct ListEmployee : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  ListEmployee() = default;

  // Make sure to initialize parsing first for new structure
  ListEmployee(ListEmployee&& other) { *this = std::move(other); }

  ListEmployee(const ListEmployee& other) { *this = other; }

  ListEmployee& operator=(ListEmployee&& other) = default;

  ListEmployee& operator=(const ListEmployee& other) = default;

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
  REGISTER_STRUCT_FIELD_OPTIONAL(company_id, std::string, "company_id");
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

#ifdef USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
struct ListEmployeeWithSubcompany : public ListEmployee {
  auto Introspect() {
    return std::tuple_cat(ListEmployee::Introspect(), std::tie(subcompany));
  }

  ListEmployeeWithSubcompany& operator=(
      const ListEmployeeWithSubcompany& other) = default;

  REGISTER_STRUCT_FIELD(subcompany, std::string, "subcompany");
};
#endif

#ifdef USE_ATTENDANCE_LIST_ITEM
struct AttendanceListItem : public JsonCompatible {
  AttendanceListItem() = default;

  AttendanceListItem(AttendanceListItem&& other) { *this = std::move(other); }

  AttendanceListItem& operator=(AttendanceListItem&& other) = default;

  auto Introspect() {
    return std::tie(start_date, end_date, abscence_type, employee);
  }

  REGISTER_STRUCT_FIELD_OPTIONAL(start_date,
                                 userver::storages::postgres::TimePoint,
                                 "start_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(end_date,
                                 userver::storages::postgres::TimePoint,
                                 "end_date");
  REGISTER_STRUCT_FIELD(employee, ListEmployeeWithSubcompany, "employee");
  REGISTER_STRUCT_FIELD_OPTIONAL(abscence_type, std::string, "abscence_type");
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

#ifdef USE_UPLOAD_DOCUMENT_REQUEST
struct UploadDocumentRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(extension, std::string, "extension", ".pdf");
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

  auto Introspect() {
    return std::tie(id, name, type, sign_required, description, is_signed,
                    parent_id);
  }

  REGISTER_STRUCT_FIELD(id, std::string, "id");
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD_OPTIONAL(type, std::string, "type");
  REGISTER_STRUCT_FIELD(sign_required, bool, "sign_required", false);
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(is_signed, bool, "signed");
  REGISTER_STRUCT_FIELD_OPTIONAL(parent_id, std::string, "parent_id");
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

#ifdef USE_DOCUMENTS_LIST_ALL_RESPONSE
struct DocumentsListAllResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(documents, std::vector<DocumentItem>, "documents");
};
#endif

#ifdef USE_SIGN_ITEM
struct SignItem : public JsonCompatible {
  SignItem() = default;

  SignItem(SignItem&& other) { *this = std::move(other); }

  SignItem& operator=(SignItem&& other) = default;

  auto Introspect() { return std::tie(employee, is_signed); }

  REGISTER_STRUCT_FIELD(employee, ListEmployee, "employee");
  REGISTER_STRUCT_FIELD(is_signed, bool, "signed");
};
#endif

#ifdef USE_DOCUMENTS_GET_SIGNS_RESPONSE
struct DocumentsGetSignsResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(signs, std::vector<SignItem>, "signs");
};
#endif

#ifdef USE_USER_ACTION
struct UserAction : public JsonCompatible {
  UserAction() = default;

  UserAction(UserAction&& other) { *this = std::move(other); }

  UserAction& operator=(UserAction&& other) = default;

  auto Introspect() {
    return std::tie(id, type, start_date, end_date, status,
                    blocking_actions_ids);
  }

  REGISTER_STRUCT_FIELD(id, std::string, "id");
  REGISTER_STRUCT_FIELD(type, std::string, "type");
  REGISTER_STRUCT_FIELD(start_date, userver::storages::postgres::TimePoint,
                        "start_date");
  REGISTER_STRUCT_FIELD(end_date, userver::storages::postgres::TimePoint,
                        "end_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(status, std::string, "status");
  REGISTER_STRUCT_FIELD(blocking_actions_ids, std::vector<std::string>,
                        "blocking_actions_ids");
};
#endif

#ifdef USE_ACTIONS_RESPONSE
struct ActionsResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(actions, std::vector<UserAction>, "actions");
};
#endif

#ifdef USE_ACTIONS_REQUEST
struct ActionsRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(from, userver::storages::postgres::TimePoint, "from");
  REGISTER_STRUCT_FIELD(to, userver::storages::postgres::TimePoint, "to");
  REGISTER_STRUCT_FIELD_OPTIONAL(employee_id, std::string, "employee_id");
};
#endif

#ifdef USE_SUPERUSER_COMPANY_ADD_REQUEST
struct SuperuserCompanyAddRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(company_id, std::string, "company_id");
  REGISTER_STRUCT_FIELD(company_name, std::string, "company_name");
};
#endif

#ifdef USE_AUTHORIZE_REQUEST
struct AuthorizeRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(login, std::string, "login");
  REGISTER_STRUCT_FIELD(password, std::string, "password");
  REGISTER_STRUCT_FIELD(company_id, std::string, "company_id");
};
#endif

#ifdef USE_AUTHORIZE_RESPONSE
struct AuthorizeResponse : public JsonCompatible {
  AuthorizeResponse(const std::string& token_, const std::string& role_) {
    token = token_;
    role = role_;
  }

  REGISTER_STRUCT_FIELD(token, std::string, "token");
  REGISTER_STRUCT_FIELD(role, std::string, "role");
};
#endif

#ifdef USE_PYSERVICE_DOCUMENT_GENERATE_REQUEST
struct PyserviceDocumentGenerateRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(action_type, std::string, "action_type");
  REGISTER_STRUCT_FIELD(request_type, std::string, "request_type");
  REGISTER_STRUCT_FIELD(employee_id, std::string, "employee_id");
  REGISTER_STRUCT_FIELD(employee_name, std::string, "employee_name");
  REGISTER_STRUCT_FIELD(employee_surname, std::string, "employee_surname");
  REGISTER_STRUCT_FIELD(subcompany, std::string, "subcompany");
  REGISTER_STRUCT_FIELD(company_id, std::string, "company_id");
  REGISTER_STRUCT_FIELD(head_name, std::string, "head_name");
  REGISTER_STRUCT_FIELD(head_surname, std::string, "head_surname");
  REGISTER_STRUCT_FIELD(start_date, std::string, "start_date");
  REGISTER_STRUCT_FIELD(end_date, std::string, "end_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(employee_patronymic, std::string,
                                 "employee_patronymic");
  REGISTER_STRUCT_FIELD_OPTIONAL(head_patronymic, std::string,
                                 "head_patronymic");
  REGISTER_STRUCT_FIELD_OPTIONAL(employee_position, std::string,
                                 "employee_position");
  REGISTER_STRUCT_FIELD_OPTIONAL(head_position, std::string, "head_position");
  REGISTER_STRUCT_FIELD_OPTIONAL(first_start_date, std::string,
                                 "first_start_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(first_end_date, std::string, "first_end_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(second_start_date, std::string,
                                 "second_start_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(second_end_date, std::string,
                                 "second_end_date");
};
#endif

#ifdef USE_ABSCENCE_VERDICT_REQUEST
struct AbscenceVerdictRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(action_id, std::string, "action_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(notification_id, std::string,
                                 "notification_id");
  REGISTER_STRUCT_FIELD(approve, bool, "approve");
};
#endif

#ifdef USE_PYSERVICE_DOCUMENT_SIGN_REQUEST
struct PyserviceDocumentSignRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(employee_id, std::string, "employee_id");
  REGISTER_STRUCT_FIELD(employee_name, std::string, "employee_name");
  REGISTER_STRUCT_FIELD(employee_surname, std::string, "employee_surname");
  REGISTER_STRUCT_FIELD_OPTIONAL(employee_patronymic, std::string,
                                 "employee_patronymic");
  REGISTER_STRUCT_FIELD(subcompany, std::string, "subcompany");
  REGISTER_STRUCT_FIELD(file_key, std::string, "file_key");
  REGISTER_STRUCT_FIELD(signed_file_key, std::string, "signed_file_key");
};
#endif

#ifdef USE_ABSCENCE_REQUEST_REQUEST
struct AbscenceRequestRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(start_date, userver::storages::postgres::TimePoint,
                        "start_date");
  REGISTER_STRUCT_FIELD(end_date, userver::storages::postgres::TimePoint,
                        "end_date");
  REGISTER_STRUCT_FIELD(type, std::string, "type");
};
#endif

#ifdef USE_ABSCENCE_REQUEST_RESPONSE
struct AbscenceRequestResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(action_id, std::string, "action_id");
};
#endif

#ifdef USE_INVENTORY_ITEM
struct InventoryItemPg {
  std::string name;
  std::string description;
  std::string id;
};

struct InventoryItem : public JsonCompatible {
  InventoryItem() = default;
  InventoryItem(const InventoryItemPg& pg) {
    name = pg.name;
    description = pg.description;
    id = pg.id;
  }

  InventoryItem(InventoryItem&& other) { *this = std::move(other); }

  InventoryItem& operator=(InventoryItem&& other) = default;

  auto Introspect() { return std::tie(name, description, id); }

  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(id, std::string, "id");
};

template <>
struct userver::storages::postgres::io::CppToUserPg<InventoryItemPg> {
  static constexpr DBTypeName postgres_name = "wd_general.inventory_item";
};
#endif

#ifdef USE_INVENTORY_ADD_REQUEST
struct InventoryAddRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(item, InventoryItem, "item");
  REGISTER_STRUCT_FIELD(employee_id, std::string, "employee_id");
};
#endif

#ifdef USE_EMPLOYEE
struct Employee : public JsonCompatible {
  Employee() = default;

  Employee(Employee&& other) { *this = std::move(other); }

  Employee& operator=(Employee&& other) = default;

  auto Introspect() {
    return std::tie(id, name, surname, patronymic, photo_link, phones, email,
                    birthday, password, head_id, telegram_id, vk_id, team,
                    head_info, inventory);
  }

  REGISTER_STRUCT_FIELD(id, std::string, "id");
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD(surname, std::string, "surname");
  REGISTER_STRUCT_FIELD_OPTIONAL(patronymic, std::string, "patronymic");
  REGISTER_STRUCT_FIELD_OPTIONAL(photo_link, std::string, "photo_link");
  REGISTER_STRUCT_FIELD_OPTIONAL(phones, std::vector<std::string>, "phones");
  REGISTER_STRUCT_FIELD_OPTIONAL(email, std::string, "email");
  REGISTER_STRUCT_FIELD_OPTIONAL(birthday, std::string, "birthday");
  REGISTER_STRUCT_FIELD_OPTIONAL(password, std::string, "password");
  REGISTER_STRUCT_FIELD_OPTIONAL(head_id, std::string, "head_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(telegram_id, std::string, "telegram_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(vk_id, std::string, "vk_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(team, std::string, "team");
  REGISTER_STRUCT_FIELD_OPTIONAL(head_info, ListEmployee, "head_info");
  REGISTER_STRUCT_FIELD_OPTIONAL(inventory, std::vector<InventoryItem>,
                                 "inventory");
};
#endif
