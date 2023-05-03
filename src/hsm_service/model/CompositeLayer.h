#pragma once

#include "Extent.h"
#include "Uuid.h"

#include <map>

namespace hestia {
enum class ExtentMatchCode { EM_ERROR, EM_NONE, EM_PARTIAL, EM_FULL };

enum class ExtentMatchType { EMT_INTERSECT, EMT_MERGE };

class MarkableExtent : public hestia::Extent {
  public:
    MarkableExtent() : hestia::Extent() {}

    MarkableExtent(std::size_t offset, std::size_t length) :
        hestia::Extent(offset, length)
    {
    }

    MarkableExtent(const hestia::Extent& extent) : hestia::Extent(extent) {}
    bool m_marked_for_delete{false};
};

class CompositeLayer {
  public:
    int add_extent(
        const hestia::Extent& extent_in, bool is_write, bool overwrite);

    int add_merge_read_extent(const hestia::Extent& extent_in);

    void delete_marked_extents(bool is_write);

    std::string dump_extents(bool details, bool is_write);

    int extent_substract(
        const hestia::Extent& extent_in, bool is_write, bool* layer_empty);

    using ExtentList = std::map<std::size_t, MarkableExtent>;
    ExtentList* get_extents(bool is_write);

    bool has_read_extents() const;

    bool has_write_extents() const;

    int mark_for_deletion(const hestia::Extent& extent_in, bool is_write);

    ExtentMatchCode match_extent(
        const hestia::Extent& extent_in,
        hestia::Extent* match,
        ExtentMatchType mode,
        bool is_write,
        bool delete_previous);

    bool operator<(const CompositeLayer& other) const
    {
        return m_priority < other.m_priority;
    }

    hestia::Uuid m_id;
    hestia::Uuid m_parent_id;
    uint32_t m_priority{0};

  private:
    ExtentList m_read_extents;
    ExtentList m_write_extents;
};
}  // namespace hestia