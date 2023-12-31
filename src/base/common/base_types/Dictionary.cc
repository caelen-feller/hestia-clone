#include "Dictionary.h"

#include "StringUtils.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace hestia {
Dictionary::Dictionary(Dictionary::Type type) : m_type(type) {}

Dictionary::Dictionary(const Dictionary& other)
{
    copy_from(other);
}

Dictionary::Ptr Dictionary::create(Dictionary::Type type)
{
    return std::make_unique<Dictionary>(type);
}

void Dictionary::add_sequence_item(std::unique_ptr<Dictionary> item)
{
    m_sequence.push_back(std::move(item));
}

void Dictionary::set_type(Type type)
{
    m_type = type;
}

void Dictionary::for_each_scalar(onItem func) const
{
    if (m_type == Type::MAP) {
        for (const auto& [key, value] : m_map) {
            if (value->get_type() == Type::SCALAR) {
                func(key, value->get_scalar());
            }
        }
    }
}

void Dictionary::merge(const Dictionary& dict)
{
    if (dict.is_empty()) {
        return;
    }

    if (is_empty()) {
        copy_from(dict);
    }

    if (dict.get_type() == Dictionary::Type::SCALAR
        && m_type == Dictionary::Type::SCALAR) {
        m_scalar = dict.m_scalar;
    }
    else if (
        dict.get_type() == Dictionary::Type::SEQUENCE
        && m_type == Dictionary::Type::SEQUENCE) {
        for (const auto& item : dict.get_sequence()) {
            m_sequence.push_back(std::make_unique<Dictionary>(*item));
        }
    }
    else if (
        dict.get_type() == Dictionary::Type::MAP
        && m_type == Dictionary::Type::MAP) {
        for (const auto& [key, value] : dict.get_map()) {
            if (const auto iter = m_map.find(key); iter == m_map.end()) {
                m_map.emplace(key, std::make_unique<Dictionary>(*value));
            }
            else {
                iter->second = std::make_unique<Dictionary>(*value);
            }
        }
    }
    else {
        throw std::runtime_error("Incompatible dict types in merge operation");
    }
}

void flatten_layer(
    const Dictionary& dict,
    Map& flat_representation,
    const std::string& prefix,
    const std::string& delim)
{
    std::string spacer = delim;
    if (prefix.empty()) {
        spacer = "";
    }

    if (dict.get_type() == Dictionary::Type::SCALAR) {
        flat_representation.set_item(
            prefix.empty() ? "value" : prefix, dict.get_scalar());
    }
    else if (dict.get_type() == Dictionary::Type::SEQUENCE) {
        std::size_t count = 0;
        for (const auto& item : dict.get_sequence()) {
            flatten_layer(
                *item, flat_representation,
                prefix + spacer + "seq_" + std::to_string(count), delim);
            count++;
        }
    }
    else {
        for (const auto& [key, value] : dict.get_map()) {
            flatten_layer(
                *value, flat_representation, prefix + spacer + key, delim);
        }
    }
}

void Dictionary::flatten(Map& flat_representation) const
{
    if (is_empty()) {
        return;
    }

    flatten_layer(*this, flat_representation, "", m_delim);
}

std::string Dictionary::to_string(bool sort_keys) const
{
    Map flat;
    flatten(flat);
    return flat.to_string(sort_keys);
}

void Dictionary::expand(const Map& flat_representation)
{
    if (flat_representation.empty()) {
        return;
    }

    std::vector<std::string> keys;
    for (const auto& [key, value] : flat_representation.data()) {
        keys.push_back(key);
    }

    std::vector<std::vector<std::string>> parsed_keys(keys.size());

    std::size_t count{0};
    for (const auto& key : keys) {
        StringUtils::split(key, m_delim, parsed_keys[count]);
        count++;
    }

    std::sort(
        parsed_keys.begin(), parsed_keys.end(),
        [](const std::vector<std::string>& a,
           const std::vector<std::string>& b) { return a.size() < b.size(); });

    for (const auto& parsed_keyset : parsed_keys) {
        Dictionary* working_dict = this;
        std::size_t count        = 1;
        std::string full_path;
        for (const auto& entry : parsed_keyset) {
            const bool is_last = count == parsed_keyset.size();
            if (is_last) {
                full_path += entry;
            }
            else {
                full_path += entry + m_delim;
            }
            if (StringUtils::starts_with(entry, "seq_")) {
                if (working_dict->get_type() != Dictionary::Type::SEQUENCE) {
                    working_dict->set_type(Dictionary::Type::SEQUENCE);
                }
                auto dict                = std::make_unique<Dictionary>();
                Dictionary* next_working = dict.get();
                working_dict->add_sequence_item(std::move(dict));
                working_dict = next_working;
            }
            else {
                if (working_dict->has_map_item(entry)) {
                    working_dict = working_dict->get_map_item(entry);
                }
                else {
                    if (is_last) {
                        auto dict = std::make_unique<Dictionary>(
                            Dictionary::Type::SCALAR);
                        dict->set_scalar(
                            flat_representation.get_item(full_path));
                        working_dict->set_map_item(entry, std::move(dict));
                    }
                    else {
                        auto dict = std::make_unique<Dictionary>();
                        Dictionary* next_working = dict.get();
                        working_dict->set_map_item(entry, std::move(dict));
                        working_dict = next_working;
                    }
                }
            }
            count++;
        }
    }
}

std::string Dictionary::get_scalar(
    const std::string& key, const std::string& delimiter) const
{
    if (m_type != Type::MAP) {
        return {};
    }

    std::vector<std::string> nested_keys;
    StringUtils::split(key, delimiter, nested_keys);

    Dictionary* working_dict{nullptr};
    std::size_t depth_count = 0;
    for (const auto& key : nested_keys) {
        if (working_dict == nullptr) {
            working_dict = get_map_item(key);
        }
        else {
            working_dict = working_dict->get_map_item(key);
        }

        if (working_dict == nullptr) {
            return {};
        }

        if (working_dict->get_type() == Type::SCALAR) {
            if (depth_count == nested_keys.size() - 1) {
                break;
            }
            else {
                return {};
            }
        }
        else if (working_dict->get_type() != Type::MAP) {
            return {};
        }
        depth_count++;
    }

    if (working_dict != nullptr) {
        return working_dict->get_scalar();
    }
    return {};
}

bool Dictionary::has_key_and_value(
    const KeyValuePair& kv_pair, const std::string& delimiter) const
{
    return get_scalar(kv_pair.first, delimiter) == kv_pair.second;
}

void Dictionary::get_map_items(
    Map& sink, const std::vector<std::string>& exclude_keys) const
{
    if (m_type == Type::MAP) {
        for (const auto& [key, value] : m_map) {
            if (value->get_type() == Type::SCALAR
                && std::find(exclude_keys.begin(), exclude_keys.end(), key)
                       == exclude_keys.end()) {
                sink.set_item(key, value->get_scalar());
            }
        }
    }
}

void Dictionary::get_scalars(
    const std::string& key, std::vector<std::string>& values) const
{
    if (m_type == Type::SCALAR) {
        values.push_back(get_scalar());
    }
    else if (m_type == Type::MAP) {
        for (const auto& [map_key, value] : m_map) {
            if (map_key == key && value->get_type() == Type::SCALAR) {
                values.push_back(value->get_scalar());
                break;
            }
        }
    }
    else {
        for (const auto& seq_item : m_sequence) {
            seq_item->get_scalars(key, values);
        }
    }
}

Dictionary* Dictionary::get_map_item(const std::string& key) const
{
    if (auto iter = m_map.find(key); iter != m_map.end()) {
        return iter->second.get();
    }
    return nullptr;
}

Dictionary::Type Dictionary::get_type() const
{
    return m_type;
}

const std::string& Dictionary::get_scalar() const
{
    return m_scalar;
}

bool Dictionary::has_map_item(const std::string& key) const
{
    auto iter = m_map.find(key);
    return iter != m_map.end();
}

const std::vector<std::unique_ptr<Dictionary>>& Dictionary::get_sequence() const
{
    return m_sequence;
}

const std::unordered_map<std::string, std::unique_ptr<Dictionary>>&
Dictionary::get_map() const
{
    return m_map;
}

void Dictionary::set_map_item(
    const std::string& key, std::unique_ptr<Dictionary> item)
{
    m_map[key] = std::move(item);
}

void Dictionary::set_map(
    const std::unordered_map<std::string, std::string>& items)
{
    for (const auto& [key, value] : items) {
        auto dict = Dictionary::create(Dictionary::Type::SCALAR);
        dict->set_scalar(value);
        set_map_item(key, std::move(dict));
    }
}

void Dictionary::set_scalar(const std::string& scalar, bool should_quote)
{
    m_scalar       = scalar;
    m_should_quote = should_quote;
}

bool Dictionary::should_quote_scalar() const
{
    return m_should_quote;
}

bool Dictionary::has_tag() const
{
    return !m_tag.second.empty();
}

const std::pair<std::string, std::string>& Dictionary::get_tag() const
{
    return m_tag;
}

void Dictionary::set_tag(const std::string& tag, const std::string& prefix)
{
    m_tag.first.assign(prefix);
    m_tag.second.assign(tag);
}

bool Dictionary::is_empty() const
{
    switch (m_type) {
        case Type::MAP:
            return m_map.empty();
        case Type::SEQUENCE:
            return m_sequence.empty();
        case Type::SCALAR:
            return m_scalar.empty();
    }
    return true;
}

void Dictionary::copy_from(const Dictionary& other)
{
    m_type   = other.m_type;
    m_tag    = other.m_tag;
    m_scalar = other.m_scalar;
    m_sequence.resize(other.m_sequence.size());

    for (std::size_t idx = 0; idx < m_sequence.size(); idx++) {
        m_sequence[idx] =
            std::make_unique<Dictionary>(*(other.m_sequence[idx]));
    }

    for (const auto& [key, value] : other.m_map) {
        m_map.emplace(key, std::make_unique<Dictionary>(*value));
    }
}

Dictionary& Dictionary::operator=(const Dictionary& other)
{
    if (this == &other) {
        return *this;
    }
    copy_from(other);

    return *this;
}

bool Dictionary::operator==(const Dictionary& rhs) const
{
    if (m_type != rhs.get_type()) {
        return false;
    }

    if (m_tag != rhs.get_tag()) {
        return false;
    }

    if (m_type == Dictionary::Type::MAP) {
        for (const auto& [key, value] : m_map) {
            if (!rhs.has_map_item(key)) {
                return false;
            }
            if (value == nullptr || rhs.get_map_item(key) == nullptr) {
                if (value != nullptr || rhs.get_map_item(key) != nullptr) {
                    return false;
                }
            }
            if (value != nullptr) {
                if (*value == *(rhs.get_map_item(key))) {
                    return false;
                }
            }
        }
    }
    else if (m_type == Dictionary::Type::SEQUENCE) {
        if (m_sequence.size() != rhs.get_sequence().size()) {
            return false;
        }

        for (size_t i = 0; i < m_sequence.size(); i++) {
            if (!(*(m_sequence[i]) == *(rhs.get_sequence()[i]))) {
                return false;
            }
        }
    }
    else if (m_type == Dictionary::Type::SCALAR) {
        return m_scalar == rhs.get_scalar();
    }

    return true;
}

}  // namespace hestia