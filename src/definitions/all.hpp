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

#ifdef USE_SEARCH_BASIC_REQUEST
#define USE_TRACKER_TASKS_LIST_ITEM
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
#define USE_TRACKER_TASKS_LIST_ITEM
#define USE_LIST_EMPLOYEE
#define USE_TRACKER_PROJECTS_LIST_ITEM
#endif

#ifdef V1_ATTENDANCE_LIST_ALL
#define USE_ATTENDANCE_LIST_ALL_REQUEST
#define USE_ATTENDANCE_LIST_ALL_RESPONSE
#endif

#ifdef V1_ATTENDANCE_EXPORT_TO_EXCEL
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
#define USE_PYSERVICE_DOCUMENT_SEND_REQUEST
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

#ifdef USE_DOCUMENT_ITEM
#define USE_DOCUMENTS_CHAIN_METADATA_ITEM
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
#define USE_ABSCENCE_VERDICT_RESPONSE
#define USE_PYSERVICE_DOCUMENT_SIGN_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_DOCUMENTS_GENERATE_FROM_TEMPLATE
#define USE_PYSERVICE_DOCUMENT_GENERATE_REQUEST
#define USE_GENERATE_FROM_TEMPLATE_REQUEST
#define USE_GENERATE_FROM_TEMPLATE_RESPONSE
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_DOCUMENTS_SIGN
#define USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
#define USE_PYSERVICE_DOCUMENT_SIGN_REQUEST
#define USE_ABSCENCE_VERDICT_RESPONSE
#define USE_ERROR_MESSAGE
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

#ifdef V1_MESSENGER_CREATE_CHAT
#define USE_CREATE_CHAT_REQUEST
#define USE_CREATE_CHAT_RESPONSE
#endif

#ifdef V1_MESSENGER_INFO
#define USE_MESSAGES
#define USE_MESSENGER_LISTED_CHAT_INFO
#define USE_MESSENGER_LIST_ALL_CHATS
#define USE_LOAD_RECENT_MESSAGES_REQUEST
#endif

#ifdef USE_MESSAGES
#define USE_MESSENGER_MESSAGE_CONTENT
#define USE_MESSENGER_MESSAGE
#endif

#ifdef V1_TRACKER_PROJECTS_ADD
#define USE_TRACKER_PROJECTS_ADD_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_TRACKER_PROJECTS_LIST
#define USE_TRACKER_PROJECTS_LIST_ITEM
#define USE_TRACKER_PROJECTS_LIST_RESPONSE
#endif

#ifdef V1_TRACKER_TASKS_ADD
#define USE_REVERSE_INDEX
#define USE_TRACKER_TASKS_ITEM_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_TRACKER_TASKS_LIST
#define USE_TRACKER_TASKS_LIST_ITEM
#define USE_TRACKER_TASKS_LIST_RESPONSE
#endif

#ifdef V1_TRACKER_TASKS_INFO
#define USE_TRACKER_TASKS_INFO_ITEM
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_TRACKER_TASKS_ASSIGNED_TO_USER
#define USE_TRACKER_TASKS_LIST_ITEM
#define USE_TRACKER_TASKS_LIST_RESPONSE
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_TRACKER_TASKS_EDIT
#define USE_REVERSE_INDEX
#define USE_TRACKER_TASKS_EDIT_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_TRACKER_PROJECTS_EDIT
#define USE_REVERSE_INDEX
#define USE_TRACKER_PROJECTS_EDIT_REQUEST
#define USE_ERROR_MESSAGE
#endif


#ifdef V1_TRACKER_PROJECTS_INFO
#define USE_TRACKER_PROJECTS_LIST_ITEM
#define USE_ERROR_MESSAGE
#endif


#ifdef V1_TRACKER_TASKS_MEDIA_UPLOAD
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_TRACKER_TASKS_DOCUMENTS_SEND
#define USE_TRACKER_TASKS_DOCUMENT_ITEM
#define USE_PYSERVICE_DOCUMENT_SEND_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_TRACKER_TASKS_DOCUMENTS_REMOVE
#define USE_ERROR_MESSAGE
#endif

#ifdef V1_DOCUMENTS_CHAIN_UPDATE
#define USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
#define USE_PYSERVICE_DOCUMENT_SIGN_REQUEST
#define USE_DOCUMENTS_CHAIN_UPDATE_REQUEST
#define USE_DOCUMENTS_CHAIN_UPDATE_RESPONSE
#define USE_ERROR_MESSAGE
#endif

#ifdef USE_DOCUMENTS_CHAIN_UPDATE_RESPONSE
#define USE_DOCUMENTS_CHAIN_METADATA_ITEM
#endif

#ifdef V1_DOCUMENTS_CHAIN_ADD
#define USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
#define USE_DOCUMENTS_CHAIN_ADD_REQUEST
#define USE_ERROR_MESSAGE
#endif

#ifdef USE_DOCUMENTS_CHAIN_ADD_REQUEST
#define USE_DOCUMENTS_CHAIN_METADATA_ITEM
#endif

#ifdef USE_LIST_EMPLOYEE_WITH_SUBCOMPANY
#define USE_LIST_EMPLOYEE
#endif

#ifdef V1_DOCUMENTS_REMOVE
#define USE_ERROR_MESSAGE
#define USE_DOCUMENTS_REMOVE_RESTORE_ITEM
#define USE_DOCUMENTS_CHAIN_METADATA_ITEM
#endif

#ifdef V1_DOCUMENTS_RESTORE
#define USE_ERROR_MESSAGE
#define USE_DOCUMENTS_REMOVE_RESTORE_ITEM
#define USE_DOCUMENTS_CHAIN_METADATA_ITEM
#endif

#ifdef V1_DOCUMENTS_HISTORY
#define USE_ERROR_MESSAGE
#define USE_DOCUMENT_HISTORY_ITEM
#define USE_DOCUMENT_HISTORY_RESPONSE
#endif

#ifdef V1_EMPLOYEE_PERMISSIONS_LIST
#define USE_ERROR_MESSAGE
#define USE_EMPLOYEE_PERMISSIONS_ITEM
#define USE_EMPLOYEE_PERMISSIONS
#endif

#ifdef V1_EMPLOYEE_PERMISSIONS_SET
#define USE_ERROR_MESSAGE
#define USE_EMPLOYEE_PERMISSIONS_ITEM
#define USE_EMPLOYEE_PERMISSIONS
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
  REGISTER_STRUCT_FIELD_OPTIONAL(job_position, std::string, "job_position");
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
  REGISTER_STRUCT_FIELD_OPTIONAL(job_position, std::string, "job_position");
};
#endif

#ifdef USE_SEARCH_BASIC_REQUEST
struct SearchBasicRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
  REGISTER_STRUCT_FIELD_OPTIONAL(tags, std::vector<std::string>, "tags");
};
#endif

#ifdef USE_SEARCH_FULL_REQUEST
struct SearchFullRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
  REGISTER_STRUCT_FIELD(limit, int, "limit");
  REGISTER_STRUCT_FIELD_OPTIONAL(tags, std::vector<std::string>, "tags");
};
#endif

#ifdef USE_SEARCH_SUGGEST_REQUEST
struct SearchSuggestRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(search_key, std::string, "search_key");
  REGISTER_STRUCT_FIELD(limit, int, "limit");
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
    return std::tie(start_date, end_date, abscence_type, attendance_type, employee);
  }

  REGISTER_STRUCT_FIELD_OPTIONAL(start_date,
                                 userver::storages::postgres::TimePoint,
                                 "start_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(end_date,
                                 userver::storages::postgres::TimePoint,
                                 "end_date");
  REGISTER_STRUCT_FIELD_OPTIONAL(abscence_type, std::string, "abscence_type");
  REGISTER_STRUCT_FIELD_OPTIONAL(attendance_type, std::string, "attendance_type");
  REGISTER_STRUCT_FIELD(employee, ListEmployeeWithSubcompany, "employee");
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

#ifdef USE_DOCUMENTS_CHAIN_METADATA_ITEM

struct DocumentsChainMetadataItemPg {
  std::string employee_id;
  int requires_signature;
  int status;
};

struct DocumentsChainMetadataItem : public JsonCompatible {
  DocumentsChainMetadataItem() = default;
  DocumentsChainMetadataItem(const DocumentsChainMetadataItemPg& pg) {
    employee_id = pg.employee_id;
    requires_signature = pg.requires_signature;
    status = pg.status;
  }

  DocumentsChainMetadataItem(DocumentsChainMetadataItem&& other) { *this = std::move(other); }

  DocumentsChainMetadataItem& operator=(DocumentsChainMetadataItem&& other) = default;

  auto Introspect() {
    return std::tie(employee_id, requires_signature, status);
  }

  REGISTER_STRUCT_FIELD(employee_id, std::string, "employee_id");
  REGISTER_STRUCT_FIELD(requires_signature, int, "requires_signature", 0);
  REGISTER_STRUCT_FIELD(status, int, "status");
};

template <>
struct userver::storages::postgres::io::CppToUserPg<DocumentsChainMetadataItemPg> {
  static constexpr DBTypeName postgres_name = "wd_general.chain_metadata_item_new";
};
#endif

#ifdef USE_DOCUMENT_ITEM
struct DocumentItem : public JsonCompatible {
  DocumentItem() = default;

  DocumentItem(DocumentItem&& other) { *this = std::move(other); }

  DocumentItem& operator=(DocumentItem&& other) = default;

  auto Introspect() {
    return std::tie(id, name, type, sign_required, description, is_signed,
                    parent_id, created_ts, chain_metadata, visibility_status);
  }

  REGISTER_STRUCT_FIELD(id, std::string, "id");
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD_OPTIONAL(type, std::string, "type");
  REGISTER_STRUCT_FIELD(sign_required, bool, "sign_required", false);
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(is_signed, bool, "signed");
  REGISTER_STRUCT_FIELD_OPTIONAL(parent_id, std::string, "parent_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(created_ts, userver::storages::postgres::TimePoint, "created_ts");
  REGISTER_STRUCT_FIELD_OPTIONAL(chain_metadata, std::vector<DocumentsChainMetadataItem>, "chain_metadata_new");
  REGISTER_STRUCT_FIELD_OPTIONAL(visibility_status, int, "visibility_status");

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

  auto Introspect() { return std::tie(employee, is_signed, document_id); }

  REGISTER_STRUCT_FIELD(employee, ListEmployee, "employee");
  REGISTER_STRUCT_FIELD(is_signed, bool, "signed");
  REGISTER_STRUCT_FIELD_OPTIONAL(document_id, std::string, "document_id");
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
                    blocking_actions_ids, attendance_type);
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
  REGISTER_STRUCT_FIELD_OPTIONAL(attendance_type, std::string, "attendance_type");
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
  REGISTER_STRUCT_FIELD_OPTIONAL(params, std::vector<std::string>,
                                 "params");
  REGISTER_STRUCT_FIELD_OPTIONAL(head_template, std::string,
                                 "head_template");
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
  REGISTER_STRUCT_FIELD_OPTIONAL(is_first_signature, bool, "is_first_signature");
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
                    head_info, inventory, job_position);
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
  REGISTER_STRUCT_FIELD_OPTIONAL(job_position, std::string, "job_position");
};
#endif

#ifdef USE_MESSENGER_MESSAGE_CONTENT
struct MessengerMessageContent : public JsonCompatible {
  MessengerMessageContent() = default;
  MessengerMessageContent(const std::string& content) : content(content) {};

  REGISTER_STRUCT_FIELD(content, std::string, "content");
};
#endif

#ifdef USE_MESSENGER_MESSAGE
struct MessengerMessage : public JsonCompatible {
  MessengerMessage() = default;

  REGISTER_STRUCT_FIELD(chat_id, std::string, "chat_id");
  REGISTER_STRUCT_FIELD(sender_id, std::string, "sender_id");
  REGISTER_STRUCT_FIELD(content, MessengerMessageContent, "content");
  REGISTER_STRUCT_FIELD(timestamp, userver::storages::postgres::TimePoint, "timestamp");
};
#endif

#ifdef USE_MESSENGER_LISTED_CHAT_INFO
struct MessengerListedChatInfo : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  MessengerListedChatInfo() = default;

  // Make sure to initialize parsing first for new structure
  MessengerListedChatInfo(MessengerListedChatInfo&& other) { *this = std::move(other); }

  MessengerListedChatInfo(const MessengerListedChatInfo& other) { *this = other; }

  MessengerListedChatInfo& operator=(MessengerListedChatInfo&& other) = default;

  MessengerListedChatInfo& operator=(const MessengerListedChatInfo& other) = default;

  REGISTER_STRUCT_FIELD(chat_id, std::string, "chat_id");
  REGISTER_STRUCT_FIELD(chat_name, std::string, "chat_name");
  REGISTER_STRUCT_FIELD(last_message, MessengerMessage, "last_message");
};
#endif

#ifdef USE_MESSENGER_LIST_ALL_CHATS
struct MessengerListAllChats : public JsonCompatible {
  REGISTER_STRUCT_FIELD(chats, std::vector<MessengerListedChatInfo>, "chats");
};
#endif

#ifdef USE_CREATE_CHAT_REQUEST
struct CreateChatRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(chat_name, std::string, "chat_name");
  REGISTER_STRUCT_FIELD(id_list, std::vector<std::string>, "id_list");
};
#endif

#ifdef USE_CREATE_CHAT_RESPONSE
struct CreateChatResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(chat_id, std::string, "chat_id");
};
#endif

#ifdef USE_LOAD_RECENT_MESSAGES_REQUEST
struct LoadRecentMessagesRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(chat_id, std::string, "chat_id");
};
#endif

#ifdef USE_TRACKER_PROJECTS_ADD_REQUEST
struct TrackerProjectsItemRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(title, std::string, "title");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(assigned_users_ids, std::vector<std::string>, "assigned_users_ids");
  REGISTER_STRUCT_ENUM_FIELD_OPTIONAL(status, std::string, "status", {"Open", "Pause", "Closed"});
};

struct TrackerProjectsAddResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(project_id, std::string, "project_id");
};
#endif

#ifdef USE_TRACKER_PROJECTS_LIST_ITEM

struct TrackerProjectsItemResponseShort : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  TrackerProjectsItemResponseShort() = default;

  // Make sure to initialize parsing first for new structure
  TrackerProjectsItemResponseShort(TrackerProjectsItemResponseShort&& other) { *this = std::move(other); }

  TrackerProjectsItemResponseShort(const TrackerProjectsItemResponseShort& other) { *this = other; }

  TrackerProjectsItemResponseShort& operator=(TrackerProjectsItemResponseShort&& other) = default;

  TrackerProjectsItemResponseShort& operator=(const TrackerProjectsItemResponseShort& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(project_id, title, image_url, creator);
  }

  REGISTER_STRUCT_FIELD(project_id, std::string, "project_id");
  REGISTER_STRUCT_FIELD(title, std::string, "title");
  REGISTER_STRUCT_FIELD_OPTIONAL(image_url, std::string, "image_url");
  REGISTER_STRUCT_FIELD(creator, std::string, "creator");
};

struct TrackerProjectsItemResponse : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  TrackerProjectsItemResponse() = default;

  // Make sure to initialize parsing first for new structure
  TrackerProjectsItemResponse(TrackerProjectsItemResponse&& other) { *this = std::move(other); }

  TrackerProjectsItemResponse(const TrackerProjectsItemResponse& other) { *this = other; }

  TrackerProjectsItemResponse& operator=(TrackerProjectsItemResponse&& other) = default;

  TrackerProjectsItemResponse& operator=(const TrackerProjectsItemResponse& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(project_id, title, description, image_url, creator, tasks_count, status, created_ts, last_updated_ts, assigned_users_ids);
  }

  REGISTER_STRUCT_FIELD(project_id, std::string, "project_id");
  REGISTER_STRUCT_FIELD(title, std::string, "title");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(image_url, std::string, "image_url");
  REGISTER_STRUCT_FIELD(creator, std::string, "creator");
  REGISTER_STRUCT_FIELD(tasks_count, int, "tasks_count");
  REGISTER_STRUCT_ENUM_FIELD_OPTIONAL(status, std::string, "status", std::vector<std::string>{"Open", "Pause", "Closed"});
  REGISTER_STRUCT_FIELD(created_ts, userver::storages::postgres::TimePoint, "created_ts");
  REGISTER_STRUCT_FIELD(last_updated_ts, userver::storages::postgres::TimePoint, "last_updated_ts");
  REGISTER_STRUCT_FIELD_OPTIONAL(assigned_users_ids, std::vector<std::string>, "assigned_users_ids");
};
#endif

#ifdef USE_TRACKER_PROJECTS_LIST_RESPONSE
struct TrackerProjectsListResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(projects, std::vector<TrackerProjectsItemResponseShort>, "projects");
};
#endif

#ifdef USE_TRACKER_TASKS_ITEM_REQUEST
struct TrackerTasksItemRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(title, std::string, "title");
  REGISTER_STRUCT_FIELD(project_id, std::string, "project_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(assignee, std::string, "assignee");
  REGISTER_STRUCT_FIELD_OPTIONAL(deadline, userver::storages::postgres::TimePoint, "deadline");
  REGISTER_STRUCT_ENUM_FIELD_OPTIONAL(status, std::string, "status", std::vector<std::string>{"Open", "InProgress", "Review", "Done", "Canceled"});
  REGISTER_STRUCT_ENUM_FIELD_OPTIONAL(priority, std::string, "priority", std::vector<std::string>{"Low", "Middle", "High"});
  REGISTER_STRUCT_FIELD_OPTIONAL(observers, std::vector<std::string>, "observers");
  REGISTER_STRUCT_FIELD_OPTIONAL(related_tasks_ids, std::vector<std::string>, "related_tasks_ids");
};

struct TrackerTasksAddResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(task_id, std::string, "task_id");
};
#endif

#ifdef USE_TRACKER_TASKS_LIST_ITEM
struct TrackerTasksItemResponseShort : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  TrackerTasksItemResponseShort() = default;

  // Make sure to initialize parsing first for new structure
  TrackerTasksItemResponseShort(TrackerTasksItemResponseShort&& other) { *this = std::move(other); }

  TrackerTasksItemResponseShort(const TrackerTasksItemResponseShort& other) { *this = other; }

  TrackerTasksItemResponseShort& operator=(TrackerTasksItemResponseShort&& other) = default;

  TrackerTasksItemResponseShort& operator=(const TrackerTasksItemResponseShort& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(title, project_id, task_id, creator, assignee);
  }

  REGISTER_STRUCT_FIELD(title, std::string, "title");
  REGISTER_STRUCT_FIELD(project_id, std::string, "project_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(task_id, std::string, "task_id");
  REGISTER_STRUCT_FIELD(creator, std::string, "creator");
  REGISTER_STRUCT_FIELD_OPTIONAL(assignee, std::string, "assignee");
};
#endif

#ifdef USE_TRACKER_TASKS_INFO_ITEM
struct TrackerTasksItemResponse : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  TrackerTasksItemResponse() = default;

  // Make sure to initialize parsing first for new structure
  TrackerTasksItemResponse(TrackerTasksItemResponse&& other) { *this = std::move(other); }

  TrackerTasksItemResponse(const TrackerTasksItemResponse& other) { *this = other; }

  TrackerTasksItemResponse& operator=(TrackerTasksItemResponse&& other) = default;

  TrackerTasksItemResponse& operator=(const TrackerTasksItemResponse& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(task_id, title, project_id, description, creator, assignee, status,
      priority, media_links, created_ts, last_updated_ts, deadline, action_id, observers, related_tasks_ids, document_ids);
  }

  REGISTER_STRUCT_FIELD_OPTIONAL(task_id, std::string, "task_id");
  REGISTER_STRUCT_FIELD(title, std::string, "title");
  REGISTER_STRUCT_FIELD(project_id, std::string, "project_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD(creator, std::string, "creator");
  REGISTER_STRUCT_FIELD_OPTIONAL(assignee, std::string, "assignee");
  REGISTER_STRUCT_ENUM_FIELD(status, std::string, "status", {"Open", "InProgress", "Review", "Done", "Canceled"});
  REGISTER_STRUCT_ENUM_FIELD(priority, std::string, "priority", std::vector<std::string>{"Low", "Middle", "High"});
  REGISTER_STRUCT_FIELD_OPTIONAL(media_links, std::vector<std::string>, "media_links");
  REGISTER_STRUCT_FIELD(created_ts, userver::storages::postgres::TimePoint, "created_ts");
  REGISTER_STRUCT_FIELD(last_updated_ts, userver::storages::postgres::TimePoint, "last_updated_ts");
  REGISTER_STRUCT_FIELD_OPTIONAL(deadline, userver::storages::postgres::TimePoint, "deadline");
  REGISTER_STRUCT_FIELD_OPTIONAL(action_id, std::string, "action_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(observers, std::vector<std::string>, "observers");
  REGISTER_STRUCT_FIELD_OPTIONAL(related_tasks_ids, std::vector<std::string>, "related_tasks_ids");
  REGISTER_STRUCT_FIELD_OPTIONAL(document_ids, std::vector<std::string>, "document_ids");
};
#endif

#ifdef USE_TRACKER_TASKS_LIST_RESPONSE
struct TrackerTasksListResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(tasks, std::vector<TrackerTasksItemResponseShort>, "tasks");
};
#endif

#ifdef USE_TRACKER_TASKS_EDIT_REQUEST
struct TrackerTasksEditRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD_OPTIONAL(title, std::string, "title");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(project_id, std::string, "project_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(assignee, std::string, "assignee");
  REGISTER_STRUCT_ENUM_FIELD_OPTIONAL(status, std::string, "status", {"Open", "InProgress", "Review", "Done"});
  REGISTER_STRUCT_ENUM_FIELD_OPTIONAL(priority, std::string, "priority", {"Low", "Middle", "High"});
  REGISTER_STRUCT_FIELD_OPTIONAL(deadline, userver::storages::postgres::TimePoint, "deadline");
  REGISTER_STRUCT_FIELD_OPTIONAL(observers, std::vector<std::string>, "observers");
  REGISTER_STRUCT_FIELD_OPTIONAL(related_tasks_ids, std::vector<std::string>, "related_tasks_ids");
};
#endif

#ifdef USE_TRACKER_TASKS_DOCUMENT_ITEM
struct TrackerTasksDocumentItem : public JsonCompatible {
  REGISTER_STRUCT_FIELD(document_id, std::string, "document_id");
  REGISTER_STRUCT_FIELD(name, std::string, "name");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_FIELD_OPTIONAL(created_ts, userver::storages::postgres::TimePoint, "created_ts");
  REGISTER_STRUCT_FIELD_OPTIONAL(visibility_status, int, "visibility_status");
};
#endif

#ifdef USE_TRACKER_PROJECTS_EDIT_REQUEST
struct TrackerProjectsEditRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD_OPTIONAL(title, std::string, "title");
  REGISTER_STRUCT_FIELD_OPTIONAL(description, std::string, "description");
  REGISTER_STRUCT_ENUM_FIELD_OPTIONAL(status, std::string, "status", {"Open", "Pause", "Closed"});
  REGISTER_STRUCT_FIELD_OPTIONAL(assigned_users_ids, std::vector<std::string>, "assigned_users_ids");
};
#endif

#ifdef USE_SEARCH_RESPONSE
struct SearchResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(employees, std::vector<ListEmployee>, "employees");
  REGISTER_STRUCT_FIELD(tasks, std::vector<TrackerTasksItemResponseShort>, "tasks");
  REGISTER_STRUCT_FIELD(projects, std::vector<TrackerProjectsItemResponseShort>, "projects");
};
#endif

#ifdef USE_PYSERVICE_DOCUMENT_SEND_REQUEST
struct PyserviceDocumentSendRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(file_key, std::string, "file_key");
  REGISTER_STRUCT_FIELD(converted_file_key, std::string, "converted_file_key");
};
#endif

#ifdef USE_DOCUMENTS_CHAIN_UPDATE_REQUEST
struct DocumentsChainUpdateRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(approval_status, int, "approval_status");
};
#endif

#ifdef USE_DOCUMENTS_CHAIN_UPDATE_RESPONSE
struct DocumentsChainUpdateResponse : public JsonCompatible {
  DocumentsChainUpdateResponse() = default;

  DocumentsChainUpdateResponse(DocumentsChainUpdateResponse&& other) { *this = std::move(other); }
  DocumentsChainUpdateResponse& operator=(DocumentsChainUpdateResponse&& other) = default;

  auto Introspect() {
    return std::tie(chain_metadata);
  }
  REGISTER_STRUCT_FIELD(chain_metadata, std::vector<DocumentsChainMetadataItem>, "chain_metadata_new");
};
#endif

#ifdef USE_DOCUMENTS_CHAIN_ADD_REQUEST
struct DocumentsChainAddRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD(chain_metadata, std::vector<DocumentsChainMetadataItem>, "chain_metadata_new");
};
#endif

#ifdef USE_DOCUMENTS_REMOVE_RESTORE_ITEM
struct DocumentsRemoveRestoreItem : public JsonCompatible {
  REGISTER_STRUCT_FIELD(document_id, std::string, "document_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(comment, std::string, "comment");
};
#endif

#ifdef USE_DOCUMENT_HISTORY_ITEM
struct DocumentHistoryItem : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  DocumentHistoryItem() = default;

  // Make sure to initialize parsing first for new structure
  DocumentHistoryItem(DocumentHistoryItem&& other) { *this = std::move(other); }

  DocumentHistoryItem(const DocumentHistoryItem& other) { *this = other; }

  DocumentHistoryItem& operator=(DocumentHistoryItem&& other) = default;

  DocumentHistoryItem& operator=(const DocumentHistoryItem& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(actor_id, action_type, comment, created_ts);
  }

  REGISTER_STRUCT_FIELD(actor_id, std::string, "actor_id");
  REGISTER_STRUCT_FIELD(action_type, std::string, "action_type");
  REGISTER_STRUCT_FIELD_OPTIONAL(comment, std::string, "comment");
  REGISTER_STRUCT_FIELD(created_ts, userver::storages::postgres::TimePoint, "created_ts");
};
#endif

#ifdef USE_DOCUMENT_HISTORY_RESPONSE
struct DocumentHistoryResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(history, std::vector<DocumentHistoryItem>, "history");
};
#endif

#ifdef USE_EMPLOYEE_PERMISSIONS_ITEM
struct EmployeePermissionsItem : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  EmployeePermissionsItem() = default;

  // Make sure to initialize parsing first for new structure
  EmployeePermissionsItem(EmployeePermissionsItem&& other) { *this = std::move(other); }

  EmployeePermissionsItem(const EmployeePermissionsItem& other) { *this = other; }

  EmployeePermissionsItem& operator=(EmployeePermissionsItem&& other) = default;

  EmployeePermissionsItem& operator=(const EmployeePermissionsItem& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(permission_type, permission_value);
  }

  REGISTER_STRUCT_ENUM_FIELD(permission_type, std::string, "permission_type", {"can_remove_documents" /*, etc*/ });
  REGISTER_STRUCT_FIELD(permission_value, int, "permission_value");
};
#endif

#ifdef USE_EMPLOYEE_PERMISSIONS
struct EmployeePermissions : public JsonCompatible {
  REGISTER_STRUCT_FIELD(permissions, std::vector<EmployeePermissionsItem>, "permissions");
};
#endif

#ifdef USE_GENERATE_FROM_TEMPLATE_REQUEST
struct GenerateFromTemplateRequest : public JsonCompatible {
  REGISTER_STRUCT_FIELD_OPTIONAL(params, std::vector<std::string>, "params");
};
#endif

#ifdef USE_GENERATE_FROM_TEMPLATE_RESPONSE
struct GenerateFromTemplateResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(download_link, std::string, "download_link");
};
#endif

#ifdef USE_ABSCENCE_VERDICT_RESPONSE
struct AbscenceVerdictResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(signed_file_key, std::string, "signed_file_key");
};
#endif

#ifdef USE_DOCUMENTS_REMOVE_RESTORE_ITEM
struct DocumentsRemoveRestoreItem : public JsonCompatible {
  REGISTER_STRUCT_FIELD(document_id, std::string, "document_id");
  REGISTER_STRUCT_FIELD_OPTIONAL(comment, std::string, "comment");
};
#endif

#ifdef USE_DOCUMENT_HISTORY_ITEM
struct DocumentHistoryItem : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  DocumentHistoryItem() = default;

  // Make sure to initialize parsing first for new structure
  DocumentHistoryItem(DocumentHistoryItem&& other) { *this = std::move(other); }

  DocumentHistoryItem(const DocumentHistoryItem& other) { *this = other; }

  DocumentHistoryItem& operator=(DocumentHistoryItem&& other) = default;

  DocumentHistoryItem& operator=(const DocumentHistoryItem& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(actor_id, action_type, comment, created_ts);
  }

  REGISTER_STRUCT_FIELD(actor_id, std::string, "actor_id");
  REGISTER_STRUCT_FIELD(action_type, std::string, "action_type");
  REGISTER_STRUCT_FIELD_OPTIONAL(comment, std::string, "comment");
  REGISTER_STRUCT_FIELD(created_ts, userver::storages::postgres::TimePoint, "created_ts");
};
#endif

#ifdef USE_DOCUMENT_HISTORY_RESPONSE
struct DocumentHistoryResponse : public JsonCompatible {
  REGISTER_STRUCT_FIELD(history, std::vector<DocumentHistoryItem>, "history");
};
#endif

#ifdef USE_EMPLOYEE_PERMISSIONS_ITEM
struct EmployeePermissionsItem : public JsonCompatible {
  // For postgres initialization type needs to be default constructible
  EmployeePermissionsItem() = default;

  // Make sure to initialize parsing first for new structure
  EmployeePermissionsItem(EmployeePermissionsItem&& other) { *this = std::move(other); }

  EmployeePermissionsItem(const EmployeePermissionsItem& other) { *this = other; }

  EmployeePermissionsItem& operator=(EmployeePermissionsItem&& other) = default;

  EmployeePermissionsItem& operator=(const EmployeePermissionsItem& other) = default;

  // Method for postgres initialization of non-trivial types
  auto Introspect() {
    return std::tie(permission_type, permission_value);
  }

  REGISTER_STRUCT_ENUM_FIELD(permission_type, std::string, "permission_type", {"can_remove_documents", "can_edit_employee_permissions" /*, etc*/ });
  REGISTER_STRUCT_FIELD(permission_value, int, "permission_value");
};
#endif

#ifdef USE_EMPLOYEE_PERMISSIONS
struct EmployeePermissions : public JsonCompatible {
  REGISTER_STRUCT_FIELD(permissions, std::vector<EmployeePermissionsItem>, "permissions");
};
#endif
