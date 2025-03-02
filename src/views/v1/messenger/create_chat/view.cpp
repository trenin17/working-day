#define V1_MESSENGER_CREATE_CHAT

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

namespace views::v1::messenger::create {

namespace {

class CreateChatHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-messenger-create-chat";

  CreateChatHandler(
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
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Origin"), "*");
    request.GetHttpResponse().SetHeader(
        static_cast<std::string>("Access-Control-Allow-Headers"), "*");

    CreateChatRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());
    auto company_id = ctx.GetData<std::string>("company_id");

    auto id = userver::utils::generators::GenerateUuid().substr(0, 16);

    auto query = fmt::format(
        "INSERT INTO working_day_{0}.messenger_chats(chat_id, chat_name) "
        "VALUES($1, $2) "
        "ON CONFLICT (chat_id) "
        "DO NOTHING",
        company_id);

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster, std::move(query),
        id, request_body.chat_name);

    userver::storages::postgres::ParameterStore parameters;
    std::string filter;

    parameters.PushBack(id);

    for (auto& field : request_body.id_list) {
        auto separator = (parameters.Size() == 1 ? "[" : ", ");
        parameters.PushBack(field);
        filter += fmt::format("{}${}", separator, parameters.Size());
    }

    result =
        pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                            "WITH input_data AS ( "
                            "  SELECT ARRAY" +
                                filter +
                                "] AS employee_ids, $1 AS chat_id "
                                ") "
                                "INSERT INTO working_day_" +
                                company_id +
                                ".employee_chats (employee_id, chats) "
                                "SELECT employee_id, ARRAY[chat_id] AS chats "
                                "FROM input_data, LATERAL unnest(employee_ids) AS employee_id "
                                "ON CONFLICT (employee_id) DO UPDATE "
                                "SET chats = array_append(working_day_" +
                                company_id +
                                ".employee_chats.chats, "
                                "EXCLUDED.chats[1]); ",
                            parameters);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendCreateChat(userver::components::ComponentList& component_list) {
  component_list.Append<CreateChatHandler>();
}

}  // namespace views::v1::messenger::create