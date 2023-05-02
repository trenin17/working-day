#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include "views/v1/employee/add/view.hpp"
#include "views/v1/employee/add_head/view.hpp"
#include "views/v1/employee/info/view.hpp"
#include "views/v1/employee/remove/view.hpp"
#include "views/v1/employees/view.hpp"

int main(int argc, char* argv[]) {
  auto component_list = userver::components::MinimalServerComponentList()
                            .Append<userver::server::handlers::Ping>()
                            .Append<userver::components::TestsuiteSupport>()
                            .Append<userver::components::HttpClient>()
                            .Append<userver::server::handlers::TestsControl>()
                            .Append<userver::components::Secdist>()
                            .Append<userver::components::DefaultSecdistProvider>()
                            .Append<userver::components::Postgres>("key-value")
                            .Append<userver::clients::dns::Component>();

  views::v1::employee::add::AppendAddEmployee(component_list);
  views::v1::employee::add_head::AppendAddHeadEmployee(component_list);
  views::v1::employee::info::AppendInfoEmployee(component_list);
  views::v1::employee::remove::AppendRemoveEmployee(component_list);
  views::v1::employees::AppendEmployees(component_list);

  return userver::utils::DaemonMain(argc, argv, component_list);
}
