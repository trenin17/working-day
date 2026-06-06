#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/server_monitor.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include <userver/components/logging_configurator.hpp>

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include "auth/auth_bearer.hpp"
#include "auth/user_info_cache.hpp"
#include "core/messenger/web_socket/web_socket.hpp"
#include "utils/custom_implicit_options.hpp"
#include "views/v1/abscence/request/view.hpp"
#include "views/v1/abscence/reschedule/view.hpp"
#include "views/v1/abscence/split/view.hpp"
#include "views/v1/abscence/verdict/view.hpp"
#include "views/v1/actions/view.hpp"
#include "views/v1/attendance/add/view.hpp"
#include "views/v1/attendance/list_all/view.hpp"
#include "views/v1/attendance/export_to_excel/view.hpp"
#include "views/v1/authorize/view.hpp"
#include "views/v1/clear-tasks/view.hpp"
#include "views/v1/documents/download/view.hpp"
#include "views/v1/documents/download_with_signatures/view.hpp"
#include "views/v1/documents/get_signs/view.hpp"
#include "views/v1/documents/list/view.hpp"
#include "views/v1/documents/list_all/view.hpp"
#include "views/v1/documents/send/view.hpp"
#include "views/v1/documents/nep/create_stamp_for_nep/view.hpp"
#include "views/v1/documents/upload/view.hpp"
#include "views/v1/documents/vacation/view.hpp"
#include "views/v1/documents/generate_from_template/view.hpp"
#include "views/v1/employee/add/view.hpp"
#include "views/v1/employee/add_head/view.hpp"
#include "views/v1/employee/info/view.hpp"
#include "views/v1/employee/remove/view.hpp"
#include "views/v1/employees/view.hpp"
#include "views/v1/inventory/add/view.hpp"
#include "views/v1/messenger/create_chat/view.hpp"
#include "views/v1/messenger/list_chats/view.hpp"
#include "views/v1/messenger/recent_messages/view.hpp"
#include "views/v1/notifications/view.hpp"
#include "views/v1/payments/add_bulk/view.hpp"
#include "views/v1/payments/view.hpp"
#include "views/v1/profile/edit/view.hpp"
#include "views/v1/profile/upload_photo/view.hpp"
#include "views/v1/search/basic/view.hpp"
#include "views/v1/search/full/view.hpp"
#include "views/v1/search/suggest/view.hpp"
#include "views/v1/superuser/company/add/view.hpp"
#include "views/v1/tracker/projects/add/view.hpp"
#include "views/v1/tracker/projects/list/view.hpp"
#include "views/v1/tracker/projects/media/upload/view.hpp"
#include "views/v1/tracker/projects/info/view.hpp"
#include "views/v1/tracker/projects/edit/view.hpp"
#include "views/v1/tracker/tasks/add/view.hpp"
#include "views/v1/tracker/tasks/list/view.hpp"
#include "views/v1/tracker/tasks/info/view.hpp"
#include "views/v1/tracker/tasks/assigned_to_user/view.hpp"
#include "views/v1/tracker/tasks/edit/view.hpp"
#include "views/v1/tracker/tasks/media/upload/view.hpp"
#include "views/v1/tracker/tasks/documents/send/view.hpp"
#include "views/v1/tracker/tasks/documents/remove/view.hpp"
#include "views/v1/documents/chain/update/view.hpp"
#include "views/v1/documents/chain/add/view.hpp"
#include "views/v1/documents/remove/view.hpp"
#include "views/v1/documents/restore/view.hpp"
#include "views/v1/documents/history/view.hpp"
#include "views/v1/employee/permissions/list/view.hpp"
#include "views/v1/employee/permissions/set/view.hpp"
#include "views/v1/comments/add/view.hpp"
#include "views/v1/comments/edit/view.hpp"
#include "views/v1/comments/info/view.hpp"
#include "views/v1/comments/remove/view.hpp"
#include "views/v1/employee/keys/generate/view.hpp"
#include "views/v1/documents/nep/nep_sign/view.hpp"
#include "views/v1/documents/nep_verify/view.hpp"
#include "views/v1/documents/upload_signature/view.hpp"

int main(int argc, char* argv[]) {
  Aws::SDKOptions options;
  Aws::InitAPI(options);

  userver::server::handlers::auth::RegisterAuthCheckerFactory<auth::CheckerFactory>();

  // userver::server::handlers::auth::RegisterAuthCheckerFactory(
      // "bearer", std::make_unique<auth::CheckerFactory>());

  auto component_list =
      userver::components::MinimalServerComponentList()
          .Append<userver::server::handlers::Ping>()
          .Append<userver::server::handlers::ServerMonitor>()
          .Append<userver::components::TestsuiteSupport>()
          .Append<userver::components::HttpClient>()
          .Append<userver::server::handlers::TestsControl>()
          .Append<userver::components::Secdist>()
          .Append<userver::components::DefaultSecdistProvider>()
          .Append<userver::components::Postgres>("key-value")
          .Append<userver::clients::dns::Component>()
          .Append<auth::AuthCache>()
          .Append<utils::custom_implicit_options::CustomImplicitOptions>()
          .Append<userver::components::LoggingConfigurator>();

  views::v1::employee::add::AppendAddEmployee(component_list);
  views::v1::employee::add_head::AppendAddHeadEmployee(component_list);
  views::v1::employee::info::AppendInfoEmployee(component_list);
  views::v1::employee::remove::AppendRemoveEmployee(component_list);
  views::v1::employees::AppendEmployees(component_list);
  views::v1::profile::edit::AppendProfileEdit(component_list);
  views::v1::profile::upload_photo::AppendProfileUploadPhoto(component_list);
  views::v1::authorize::AppendAuthorize(component_list);
  views::v1::abscence::request::AppendAbscenceRequest(component_list);
  views::v1::abscence::verdict::AppendAbscenceVerdict(component_list);
  views::v1::notifications::AppendNotifications(component_list);
  views::v1::actions::AppendActions(component_list);
  views::v1::documents::vacation::AppendDocumentsVacation(component_list);
  views::v1::attendance::add::AppendAttendanceAdd(component_list);
  views::v1::abscence::split::AppendAbscenceSplit(component_list);
  views::v1::abscence::reschedule::AppendAbscenceReschedule(component_list);
  views::v1::payments::add_bulk::AppendPaymentsAddBulk(component_list);
  views::v1::payments::AppendPayments(component_list);
  views::v1::clear_tasks::AppendClearTasks(component_list);
  views::v1::search_basic::AppendSearchBasic(component_list);
  views::v1::search_full::AppendSearchFull(component_list);
  views::v1::search_suggest::AppendSearchSuggest(component_list);
  views::v1::attendance::list_all::AppendAttendanceListAll(component_list);
  views::v1::attendance::export_to_excel::AppendAttendanceExportToExcel(component_list);
  views::v1::documents::upload::AppendDocumentsUpload(component_list);
  views::v1::documents::send::AppendDocumentsSend(component_list);
  views::v1::documents::list::AppendDocumentsList(component_list);
  views::v1::documents::download::AppendDocumentsDownload(component_list);
  views::v1::documents::download_with_signatures::AppendDocumentsDownloadWithSignatures(component_list);
  views::v1::documents::nep::create_stamp_for_nep::AppendDocumentsCreateStampForNep(component_list);
  views::v1::documents::list_all::AppendDocumentsListAll(component_list);
  views::v1::documents::get_signs::AppendDocumentsGetSigns(component_list);
  views::v1::documents::generate_from_template::AppendDocumentsGenerateFromTemplate(component_list);
  views::v1::superuser::company::add::AppendSuperuserCompanyAdd(component_list);
  views::v1::inventory::add::AppendInventoryAdd(component_list);
  views::v1::messenger::create::AppendCreateChat(component_list);
  views::v1::messenger::list_chats::AppendListChats(component_list);
  views::v1::messenger::recent_messages::AppendRecentMessages(component_list);
  views::v1::tracker::projects::add::AppendTrackerProjectsAdd(component_list);
  views::v1::tracker::projects::list::AppendTrackerProjectsList(component_list);
  views::v1::tracker::projects::media::upload::AppendTrackerProjectsMediaUpload(component_list);
  views::v1::tracker::projects::info::AppendTrackerProjectsInfo(component_list);
  views::v1::tracker::projects::edit::AppendTrackerProjectsEdit(component_list);
  views::v1::tracker::tasks::add::AppendTrackerTasksAdd(component_list);
  views::v1::tracker::tasks::list::AppendTrackerTasksList(component_list);
  views::v1::tracker::tasks::info::AppendTrackerTasksInfo(component_list);
  views::v1::tracker::tasks::assigned_to_user::AppendTrackerTasksAssignedToUser(component_list);
  views::v1::tracker::tasks::edit::AppendTrackerTasksEdit(component_list);
  views::v1::tracker::tasks::media::upload::AppendTrackerTasksMediaUpload(component_list);
  views::v1::tracker::tasks::documents::send::AppendTrackerTasksDocumentsSend(component_list);
  views::v1::tracker::tasks::documents::remove::AppendTrackerTasksDocumentsRemove(component_list);
  views::v1::documents::chain::update::AppendDocumentsChainUpdate(component_list);
  views::v1::documents::chain::add::AppendDocumentsChainAdd(component_list);
  views::v1::documents::remove::AppendDocumentsRemove(component_list);
  views::v1::documents::restore::AppendDocumentsRestore(component_list);
  views::v1::documents::history::AppendDocumentsHistory(component_list);
  views::v1::employee::permissions::list::AppendEmployeePermissionsList(component_list);
  views::v1::employee::permissions::set::AppendEmployeePermissionsSet(component_list);
  views::v1::comments::add::AppendCommentsAdd(component_list);
  views::v1::comments::edit::AppendCommentsEdit(component_list);
  views::v1::comments::info::AppendCommentsInfo(component_list);
  views::v1::comments::remove::AppendCommentsRemove(component_list);
  views::v1::employee::keys::generate::AppendEmployeeKeysGenerate(component_list);
  views::v1::documents::nep::nep_sign::AppendDocumentsNepSign(component_list);
  views::v1::documents::nep_verify::AppendDocumentsNepVerify(component_list);
  views::v1::documents::upload_signature::AppendDocumentsUploadSignatureView(component_list);

  core::websocket::AppendWebSocket(component_list);
  int err_code = userver::utils::DaemonMain(argc, argv, component_list);

  Aws::ShutdownAPI(options);
  return err_code;
}
