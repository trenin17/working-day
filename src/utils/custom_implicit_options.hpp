#include <userver/server/handlers/implicit_options.hpp>

namespace utils::custom_implicit_options {

class CustomImplicitOptions final
    : public userver::server::handlers::ImplicitOptions {
 public:
  CustomImplicitOptions(const userver::components::ComponentConfig& config,
                        const userver::components::ComponentContext& context,
                        bool is_monitor = false);

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& context) const override;
};

}  // namespace utils::custom_implicit_options