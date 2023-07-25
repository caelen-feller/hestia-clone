#pragma once

#include "BaseCrudRequest.h"
#include "Model.h"

namespace hestia {

class CrudRequest : public BaseCrudRequest, public MethodRequest<CrudMethod> {
  public:
    explicit CrudRequest(
        CrudMethod method,
        VecModelPtr items,
        CrudQuery::OutputFormat output_format =
            CrudQuery::OutputFormat::ATTRIBUTES,
        CrudAttributes::Format attributes_format =
            CrudAttributes::Format::JSON);

    explicit CrudRequest(
        CrudMethod method,
        const VecCrudIdentifier& ids     = {},
        const CrudAttributes& attributes = {},
        CrudQuery::OutputFormat output_format =
            CrudQuery::OutputFormat::ATTRIBUTES,
        CrudAttributes::Format attributes_format =
            CrudAttributes::Format::JSON);

    explicit CrudRequest(const CrudQuery& query);

    explicit CrudRequest(CrudMethod method, CrudLockType lock_type);

    virtual ~CrudRequest();

    using Ptr = std::unique_ptr<CrudRequest>;

    std::string method_as_string() const override;

    Model* get_item() const;

    bool has_items() const;

    const VecModelPtr& items() const;

  protected:
    VecModelPtr m_items;
};
}  // namespace hestia