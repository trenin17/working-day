#define V1_DOCUMENTS_SEND

#include "view.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

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
                .GetCluster()),
        http_client_(
            component_context.FindComponent<userver::components::HttpClient>()
                .GetHttpClient()),
        pyservice_url_(config["pyservice-url"].As<std::string>()) {}

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

    // if document id ends with docx, send request to python service. retry until 200
    if (request_body.document.id.ends_with(".docx")) {
        PyserviceDocumentSendRequest py_request;
        py_request.file_key = request_body.document.id;

        request_body.document.id =
            request_body.document.id.substr(0, request_body.document.id.size() - std::string(".docx").size()) + ".pdf";
        py_request.converted_file_key = request_body.document.id;


        auto response = http_client_.CreateRequest()
                            .post(pyservice_url_)
                            .data(py_request.ToJsonString())
                            .retry(2)  // retry once in case of error
                            .timeout(std::chrono::milliseconds{10000})
                            .perform();  // start performing the request
        response->raise_for_status();
    }

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "INSERT INTO working_day_" + company_id +
                             ".documents(id, name, "
                             "sign_required, description, parent_id) "
                             "VALUES($1, $2, $3, $4, $5) ON CONFLICT (id) DO NOTHING",
                         request_body.document.id, request_body.document.name,
                         request_body.document.sign_required,
                         request_body.document.description,
                         request_body.document.parent_id.value_or(""));

    auto notification_text = "Вам отправлен новый документ \"" +
                             request_body.document.name +
                             "\". Его можно просмотреть в разделе Документы.";

    userver::storages::postgres::ParameterStore parameters,
        parameters_notifications;
    std::string filter, filter_notifications;
    parameters_notifications.PushBack("generic");
    parameters_notifications.PushBack(notification_text);
    parameters_notifications.PushBack(user_id);
    for (const auto& employee_id : request_body.employee_ids) {
      filter += "($" + std::to_string(parameters.Size() + 1) + ", $" +
                std::to_string(parameters.Size() + 2) + ", FALSE),";
      parameters.PushBack(employee_id);
      parameters.PushBack(request_body.document.id);

      auto notification_id = userver::utils::generators::GenerateUuid();

      filter_notifications +=
          "($" + std::to_string(parameters_notifications.Size() + 1) +
          ", $1, $2, $3, $" +
          std::to_string(parameters_notifications.Size() + 2) + "),";

      parameters_notifications.PushBack(notification_id);
      parameters_notifications.PushBack(employee_id);
    }
    filter.pop_back();
    filter_notifications.pop_back();

    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                         "INSERT INTO working_day_" + company_id +
                             ".employee_document "
                             "(employee_id, document_id, is_author) "
                             "VALUES " +
                             filter,
                         parameters);

    // Добавить документ в список отправителя как автора (видит отправленные документы).
    // ON CONFLICT DO NOTHING: если отправил себе, уже есть строка с is_author=FALSE —
    // оставляем её, чтобы пользователь попадал в get-signs и мог подписать.
    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO working_day_" + company_id +
            ".employee_document(employee_id, document_id, is_author) VALUES($1, $2, TRUE) "
            "ON CONFLICT (employee_id, document_id) DO NOTHING",
        user_id, request_body.document.id);

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

  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HandlerBase>(R"(
type: object
description: Document send handler schema
additionalProperties: false
properties:
    pyservice-url:
        type: string
        description: Url of python service
)");
  }

 private:
  userver::storages::postgres::ClusterPtr pg_cluster_;
  userver::clients::http::Client& http_client_;
  std::string pyservice_url_;
};

}  // namespace

void AppendDocumentsSend(userver::components::ComponentList& component_list) {
  component_list.Append<DocumentsSendHandler>();
}

}  // namespace views::v1::documents::send