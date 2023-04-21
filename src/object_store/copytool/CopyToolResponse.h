#pragma once

#include "CopyToolError.h"

#include "HsmMiddlewareResponse.h"
#include "HsmObjectStoreResponse.h"

class CopyToolResponse : public HsmMiddlewareResponse {
  public:
    using Ptr = std::unique_ptr<CopyToolResponse>;

    enum class Status { OK, ERROR };

    CopyToolResponse();
    virtual ~CopyToolResponse() = default;

    static Ptr create();

    bool ok() const;

    void set_error(const CopyToolError& error);

    void setObjectStoreRespose(
        HsmObjectStoreResponse::Ptr object_store_response) override;

  private:
    Status m_status{Status::OK};
    CopyToolError m_error;
};