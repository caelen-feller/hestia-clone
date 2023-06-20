#pragma once

#include "HttpRequest.h"
#include "UserService.h"

namespace hestia {
class ApplicationMiddleware {
  public:
    using Ptr = std::unique_ptr<ApplicationMiddleware>;

    ApplicationMiddleware()          = default;
    virtual ~ApplicationMiddleware() = default;

    void set_user_service(UserService* service) { m_user_service = service; }

    virtual HttpResponse::Ptr call(
        const HttpRequest& request, User& user, responseProviderFunc func) = 0;

  protected:
    UserService* m_user_service{nullptr};
};
}  // namespace hestia