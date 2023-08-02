#include "ObjectStoreBackend.h"

#include <iostream>

namespace hestia {

ObjectStoreBackend::ObjectStoreBackend() :
    HsmItem(HsmItem::Type::OBJECT_STORE_BACKEND), Model(s_type)
{
    init();
}

ObjectStoreBackend::ObjectStoreBackend(ObjectStoreBackend::Type client_type) :
    HsmItem(HsmItem::Type::OBJECT_STORE_BACKEND), Model(s_type)
{
    m_backend_type.update_value(client_type);
    init();
}

ObjectStoreBackend::ObjectStoreBackend(const ObjectStoreBackend& other) :
    HsmItem(HsmItem::Type::OBJECT_STORE_BACKEND), Model(other)
{
    *this = other;
}

ObjectStoreBackend& ObjectStoreBackend::operator=(
    const ObjectStoreBackend& other)
{
    if (this != &other) {
        Model::operator=(other);
        m_backend_type      = other.m_backend_type;
        m_config            = other.m_config;
        m_plugin_path       = other.m_plugin_path;
        m_custom_identifier = other.m_custom_identifier;
        m_tier_names        = other.m_tier_names;

        m_node  = other.m_node;
        m_tiers = other.m_tiers;
        init();
    }
    return *this;
}

void ObjectStoreBackend::init()
{
    register_scalar_field(&m_backend_type);
    register_scalar_field(&m_plugin_path);
    register_map_field(&m_config);

    register_scalar_field(&m_custom_identifier);
    register_sequence_field(&m_tier_names);

    register_foreign_key_field(&m_node);
    register_many_to_many_field(&m_tiers);
}

const Dictionary& ObjectStoreBackend::get_config() const
{
    return m_config.value();
}

std::string ObjectStoreBackend::get_type()
{
    return s_type;
}

const std::string& ObjectStoreBackend::get_node_id() const
{
    return m_node.get_id();
}

ObjectStoreBackend::Type ObjectStoreBackend::get_backend() const
{
    return m_backend_type.get_value();
}

void ObjectStoreBackend::set_tier_names(
    const std::vector<std::string>& tier_names)
{
    m_tier_names.get_container_as_writeable() = tier_names;
}

const std::vector<std::string>& ObjectStoreBackend::get_tier_names() const
{
    return m_tier_names.container();
}

void ObjectStoreBackend::set_node_id(const std::string& id)
{
    m_node.set_id(id);
}

void ObjectStoreBackend::add_tier_id(const std::string& id)
{
    m_tiers.add_id(id);
}

void ObjectStoreBackend::set_tier_ids(const std::vector<std::string>& ids)
{
    m_tiers.set_ids(ids);
}

const std::vector<std::string> ObjectStoreBackend::get_tier_ids() const
{
    return m_tiers.get_ids();
}

bool ObjectStoreBackend::is_hsm() const
{
    return m_backend_type.get_value() == ObjectStoreBackend::Type::MOTR
           || m_backend_type.get_value() == ObjectStoreBackend::Type::FILE_HSM
           || m_backend_type.get_value() == ObjectStoreBackend::Type::CUSTOM_HSM
           || m_backend_type.get_value()
                  == ObjectStoreBackend::Type::MEMORY_HSM;
}

bool ObjectStoreBackend::is_plugin() const
{
    return m_backend_type.get_value() == ObjectStoreBackend::Type::MOTR
           || m_backend_type.get_value() == ObjectStoreBackend::Type::PHOBOS
           || m_backend_type.get_value() == ObjectStoreBackend::Type::S3
           || m_backend_type.get_value() == ObjectStoreBackend::Type::CUSTOM
           || m_backend_type.get_value() == ObjectStoreBackend::Type::MOCK_MOTR
           || m_backend_type.get_value()
                  == ObjectStoreBackend::Type::MOCK_PHOBOS
           || m_backend_type.get_value() == ObjectStoreBackend::Type::MOCK_S3;
}

bool ObjectStoreBackend::is_mock() const
{
    return m_backend_type.get_value() == ObjectStoreBackend::Type::MOCK_MOTR
           || m_backend_type.get_value()
                  == ObjectStoreBackend::Type::MOCK_PHOBOS
           || m_backend_type.get_value() == ObjectStoreBackend::Type::MOCK_S3;
}

bool ObjectStoreBackend::is_built_in() const
{
    return !is_plugin();
}

std::string ObjectStoreBackend::to_string() const
{
    Type_enum_string_converter type_converter;
    type_converter.init();
    return "Type: " + type_converter.to_string(m_backend_type.get_value());
}
}  // namespace hestia