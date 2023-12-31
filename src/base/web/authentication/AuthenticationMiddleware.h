#pragma once

#include "ApplicationMiddleware.h"

namespace hestia {
class AuthenticationMiddleware : public ApplicationMiddleware {
  public:
    virtual ~AuthenticationMiddleware() = default;
    virtual HttpResponse::Ptr call(
        const HttpRequest& request,
        AuthorizationContext& user,
        HttpEvent event,
        responseProviderFunc func) = 0;
};
}  // namespace hestia