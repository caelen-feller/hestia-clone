#pragma once

#include "HsmObjectStoreRequest.h"
#include "HsmObjectStoreResponse.h"

#include "ObjectStoreClient.h"

namespace hestia {
class HsmObjectStoreClient : public ObjectStoreClient {
  public:
    using Ptr = std::unique_ptr<HsmObjectStoreClient>;

    [[nodiscard]] virtual HsmObjectStoreResponse::Ptr make_request(
        const HsmObjectStoreRequest& request,
        Stream* stream = nullptr) const noexcept;

  protected:
    [[nodiscard]] ObjectStoreResponse::Ptr make_request(
        const ObjectStoreRequest& request,
        Stream* stream = nullptr) const noexcept override;

    virtual void copy(const HsmObjectStoreRequest& request) const = 0;

    virtual void get(
        const HsmObjectStoreRequest& request,
        StorageObject& object,
        Stream* stream) const = 0;

    virtual void move(const HsmObjectStoreRequest& request) const = 0;

    virtual void put(
        const HsmObjectStoreRequest& request, Stream* stream) const = 0;

    virtual void remove(const HsmObjectStoreRequest& request) const = 0;

    // hestia::ObjectStoreClient Methods
    bool exists(const StorageObject& object) const override;

    void get(StorageObject& object, const Extent& extent, Stream* stream)
        const override;

    void put(const StorageObject& object, const Extent& extent, Stream* stream)
        const override;

    void remove(const StorageObject& object) const override;

    void list(const Metadata::Query& query, std::vector<StorageObject>& fetched)
        const override;

  private:
    void on_exception(
        const HsmObjectStoreRequest& request,
        HsmObjectStoreResponse* response,
        const std::string& message = {}) const;

    void on_exception(
        const HsmObjectStoreRequest& request,
        HsmObjectStoreResponse* response,
        const hestia::ObjectStoreError& error) const;

    void on_exception(
        const HsmObjectStoreRequest& request,
        HsmObjectStoreResponse* response,
        const HsmObjectStoreError& error) const;
};
}  // namespace hestia