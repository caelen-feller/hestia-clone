#pragma once

#include "Dataset.h"
#include "S3Responses.h"
#include "StringAdapter.h"

#include <memory>

namespace hestia {
class S3DatasetAdapter : public StringAdapter {
  public:
    using Ptr = std::unique_ptr<S3DatasetAdapter>;

    S3DatasetAdapter(const std::string& metadata_prefix = {});

    static Ptr create(const std::string& metadata_prefix = {});

    virtual ~S3DatasetAdapter() = default;

    void dict_from_string(
        const std::string& input,
        Dictionary& dict,
        const std::string& key_prefix = {}) const override;

    void dict_to_string(
        const Dictionary& dict, std::string& output) const override;

    void on_list_buckets(
        const VecModelPtr& items, S3ListBucketResponse& list_bucket_response);

  private:
    std::string m_metadata_prefix;
};
}  // namespace hestia