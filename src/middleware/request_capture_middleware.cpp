#include "request_capture_middleware.hpp"
#include "containers/request_collector.hpp"
#include <userver/components/component_context.hpp>
#include <userver/server/middlewares/configuration.hpp>

namespace middleware::middleware {

class RequestCaptureMiddleware final : public userver::server::middlewares::HttpMiddlewareBase {
  public:
        RequestCaptureMiddleware(containers::RequestCollector& collector);


  private:
    void HandleRequest(userver::server::http::HttpRequest& request,
                     userver::server::request::RequestContext& context) const override;

    containers::RequestCollector& collector_;
};

class RequestCaptureMiddlewareFactory final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
  public:
    static constexpr std::string_view kName = "request-capture-middleware";

    RequestCaptureMiddlewareFactory(const userver::components::ComponentConfig& config,
                                    const userver::components::ComponentContext& context);

    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> Create(
        const userver::server::handlers::HttpHandlerBase& handler,
        userver::yaml_config::YamlConfig middleware_config) const override;

    containers::RequestCollector& collector_;

    static userver::yaml_config::Schema GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<userver::server::middlewares::HttpMiddlewareFactoryBase>(R"(
            type: object
            description: Request capture middleware factory
            additionalProperties: false
            properties: {}
        )");
        }  
};

class CustomHandlerPipelineBuilder final : public userver::server::middlewares::HandlerPipelineBuilder {
    public:
    static constexpr std::string_view kName = "custom-handler-pipeline-builder";

    CustomHandlerPipelineBuilder(const userver::components::ComponentConfig& config,
                                const userver::components::ComponentContext& context)
        : HandlerPipelineBuilder(config, context) {}

    userver::server::middlewares::MiddlewaresList BuildPipeline(
        userver::server::middlewares::MiddlewaresList server_middleware_pipeline) const override {
        auto pipeline = std::move(server_middleware_pipeline);
        pipeline.emplace_back(RequestCaptureMiddlewareFactory::kName);
        return pipeline;
    }

    static userver::yaml_config::Schema GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<userver::server::middlewares::HandlerPipelineBuilder>(R"(
            type: object
            description: Custom handler pipeline builder
            additionalProperties: false
            properties: {}
        )");
    }
};

RequestCaptureMiddleware::RequestCaptureMiddleware(containers::RequestCollector& collector)
    : collector_(collector) {}

void RequestCaptureMiddleware::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context) const {
    
    Next(request, context);
    
    collector_.Collect(request, context);
}

RequestCaptureMiddlewareFactory::RequestCaptureMiddlewareFactory(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : userver::server::middlewares::HttpMiddlewareFactoryBase(config, context),
      collector_(context.FindComponent<containers::RequestCollector>()) {}

std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase>
RequestCaptureMiddlewareFactory::Create(
    const userver::server::handlers::HttpHandlerBase&,
    userver::yaml_config::YamlConfig) const {
    return std::make_unique<RequestCaptureMiddleware>(collector_);
}

void AppendRequestCaptureMiddleware(userver::components::ComponentList& component_list) {
    component_list.Append<RequestCaptureMiddlewareFactory>();
    component_list.Append<CustomHandlerPipelineBuilder>();
    component_list.Append<containers::RequestCollector>();
}


}  // namespace middleware

