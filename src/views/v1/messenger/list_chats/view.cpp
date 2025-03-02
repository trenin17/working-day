#define V1_MESSENGER_INFO

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include "definitions/all.hpp"

#include <string>

namespace views::v1::messenger::list_chats {

namespace {

struct StringRow {
    std::vector<std::string> values;
  };

class ListChatsHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-messenger-list-chats";

  ListChatsHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("key-value")
                .GetCluster()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ctx) const override {
    // CORS
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    const auto& employee_id = request.GetArg("employee_id");
    const auto& company_id = ctx.GetData<std::string>("company_id");

    auto query = fmt::format(
        "SELECT chats "
        "FROM working_day_{0}.employee_chats "
        "WHERE employee_id = $1;",
        company_id);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster, std::move(query),
        employee_id);

    auto chat_ids = result.AsOptionalSingleRow<StringRow>(
          userver::storages::postgres::kRowTag);

    if (chat_ids.has_value()) {
      userver::storages::postgres::ParameterStore parameters;
      std::string filter;

      for (auto& field : (*chat_ids).values) {
          auto separator = (parameters.Size() == 0 ? "[" : ", ");
          parameters.PushBack(field);
          filter += fmt::format("{}${}", separator, parameters.Size());
      }

      result =
        pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                          "SELECT array_agg(m.chat_name ORDER BY u.ordinality) AS chat_names "
                          "FROM unnest(ARRAY" + filter + "]) WITH ORDINALITY AS u(chat_id, ordinality) "
                          "LEFT JOIN working_day_" + company_id + ".messenger_chats m "
                          "ON m.chat_id = u.chat_id;",
                          parameters);

      auto chat_names = result.AsSingleRow<StringRow>(userver::storages::postgres::kRowTag);

      MessengerListAllChats listed_chats;
      listed_chats.chats.reserve(chat_names.values.size());
      for (size_t i = 0; i < chat_names.values.size(); ++i) {
        MessengerListedChatInfo chat_info;
        chat_info.chat_id = std::move((*chat_ids).values[i]);
        chat_info.chat_name = std::move(chat_names.values[i]);
        listed_chats.chats.emplace_back(chat_info);
      }

      return listed_chats.ToJsonString();
    }

    return "{}";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendListChats(userver::components::ComponentList& component_list) {
  component_list.Append<ListChatsHandler>();
}

}  // namespace views::v1::messenger::list_chats