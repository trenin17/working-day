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

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include "auth/auth_bearer.hpp"
#include "auth/user_info_cache.hpp"
#include "utils/custom_implicit_options.hpp"
#include "views/v1/abscence/request/view.hpp"
#include "views/v1/abscence/reschedule/view.hpp"
#include "views/v1/abscence/split/view.hpp"
#include "views/v1/abscence/verdict/view.hpp"
#include "views/v1/actions/view.hpp"
#include "views/v1/attendance/add/view.hpp"
#include "views/v1/attendance/list_all/view.hpp"
#include "views/v1/authorize/view.hpp"
#include "views/v1/clear-tasks/view.hpp"
#include "views/v1/documents/download/view.hpp"
#include "views/v1/documents/get_signs/view.hpp"
#include "views/v1/documents/list/view.hpp"
#include "views/v1/documents/list_all/view.hpp"
#include "views/v1/documents/send/view.hpp"
#include "views/v1/documents/sign/view.hpp"
#include "views/v1/documents/upload/view.hpp"
#include "views/v1/documents/vacation/view.hpp"
#include "views/v1/employee/add/view.hpp"
#include "views/v1/employee/add_head/view.hpp"
#include "views/v1/employee/info/view.hpp"
#include "views/v1/employee/remove/view.hpp"
#include "views/v1/employees/view.hpp"
#include "views/v1/inventory/add/view.hpp"
#include "views/v1/notifications/view.hpp"
#include "views/v1/payments/add_bulk/view.hpp"
#include "views/v1/payments/view.hpp"
#include "views/v1/profile/edit/view.hpp"
#include "views/v1/profile/upload_photo/view.hpp"
#include "views/v1/search/basic/view.hpp"
#include "views/v1/search/full/view.hpp"
#include "views/v1/search/suggest/view.hpp"
#include "views/v1/superuser/company/add/view.hpp"

int main(int argc, char* argv[]) {
  Aws::SDKOptions options;
  Aws::InitAPI(options);

  userver::server::handlers::auth::RegisterAuthCheckerFactory(
      "bearer", std::make_unique<auth::CheckerFactory>());

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
          .Append<utils::custom_implicit_options::CustomImplicitOptions>();

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
  views::v1::documents::upload::AppendDocumentsUpload(component_list);
  views::v1::documents::send::AppendDocumentsSend(component_list);
  views::v1::documents::list::AppendDocumentsList(component_list);
  views::v1::documents::download::AppendDocumentsDownload(component_list);
  views::v1::documents::sign::AppendDocumentsSign(component_list);
  views::v1::documents::list_all::AppendDocumentsListAll(component_list);
  views::v1::documents::get_signs::AppendDocumentsGetSigns(component_list);
  views::v1::superuser::company::add::AppendSuperuserCompanyAdd(component_list);
  views::v1::inventory::add::AppendInventoryAdd(component_list);

  int err_code = userver::utils::DaemonMain(argc, argv, component_list);

  Aws::ShutdownAPI(options);
  return err_code;
}
