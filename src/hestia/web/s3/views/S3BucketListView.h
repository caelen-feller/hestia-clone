#pragma once

#include "S3DatasetAdapter.h"
#include "WebView.h"

namespace hestia {
class DistributedHsmService;

class S3BucketListView : public WebView {
  public:
    S3BucketListView(DistributedHsmService* service);

    HttpResponse::Ptr on_get(
        const HttpRequest& request,
        HttpEvent event,
        const AuthorizationContext& auth) override;

  private:
    DistributedHsmService* m_service{nullptr};
    std::unique_ptr<S3DatasetAdapter> m_dataset_adatper;
};
}  // namespace hestia