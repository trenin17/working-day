#define V1_DOCUMENTS_SEND

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>

#include <definitions/all.hpp>

namespace views::v1::documents::send {

namespace {

class DocumentsSendHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-v1-documents-send";

  DocumentsSendHandler(
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

    const auto& company_id = ctx.GetData<std::string>("company_id");
    auto user_id = ctx.GetData<std::string>("user_id");

    DocumentSendRequest request_body;
    request_body.ParseRegisteredFields(request.RequestBody());

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "INSERT INTO working_day_" + company_id +
                             ".documents(id, name, "
                             "sign_required, description, parent_id) "
                             "VALUES($1, $2, $3, $4, $5)",
                         request_body.document.id, request_body.document.name,
                         request_body.document.sign_required,
                         request_body.document.description,
                         request_body.document.parent_id.value_or(""));

    auto notification_text = "Вам отправлен новый документ \"" +
                             request_body.document.name +
                             "\". Его можно просмотреть в разделе Документы.";
    auto notification_id = userver::utils::generators::GenerateUuid();

    userver::storages::postgres::ParameterStore parameters,
        parameters_notifications;
    std::string filter, filter_notifications;
    parameters_notifications.PushBack(notification_id);
    parameters_notifications.PushBack("generic");
    parameters_notifications.PushBack(notification_text);
    parameters_notifications.PushBack(user_id);
    for (const auto& employee_id : request_body.employee_ids) {
      filter += "($" + std::to_string(parameters.Size() + 1) + ", $" +
                std::to_string(parameters.Size() + 2) + "),";
      parameters.PushBack(employee_id);
      parameters.PushBack(request_body.document.id);

      filter_notifications +=
          "($1, $2, $3, $4, $" +
          std::to_string(parameters_notifications.Size() + 1) + "),";
      parameters_notifications.PushBack(employee_id);
    }
    filter.pop_back();
    filter_notifications.pop_back();

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "INSERT INTO working_day_" + company_id +
                             ".employee_document "
                             "(employee_id, document_id) "
                             "VALUES " +
                             filter,
                         parameters);

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "INSERT INTO working_day_" + company_id +
                             ".notifications(id, type, "
                             "text, sender_id, user_id) "
                             "VALUES " +
                             filter_notifications +
                             " ON CONFLICT (id) "
                             "DO NOTHING",
                         parameters_notifications);

    return "";
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void AppendDocumentsSend(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsSendHandler>();
}

}  // namespace views::v1::documents::send