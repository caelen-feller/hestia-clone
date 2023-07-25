#include "Model.h"

#include "RelationshipField.h"

#include <iostream>

namespace hestia {

Model::Model(const std::string& type) : SerializeableWithFields(type, true)
{
    init();
}

Model::Model(const std::string& id, const std::string& type) :
    SerializeableWithFields(id, type)
{
    init();
}

Model::Model(const Model& other) : SerializeableWithFields(other)
{
    *this = other;
}

Model& Model::operator=(const Model& other)
{
    if (this != &other) {
        SerializeableWithFields::operator=(other);
        m_name               = other.m_name;
        m_creation_time      = other.m_creation_time;
        m_last_modified_time = other.m_last_modified_time;
        init();
    }
    return *this;
}

void Model::init()
{
    register_scalar_field(&m_name);
    register_scalar_field(&m_creation_time);
    register_scalar_field(&m_last_modified_time);
}

void Model::register_named_foreign_key_field(NamedForeignKeyField* field)
{
    m_map_fields[field->get_name()]               = field;
    m_named_foreign_key_fields[field->get_name()] = field;
}

void Model::register_foreign_key_proxy_field(DictField* field)
{
    m_sequence_fields[field->get_name()]          = field;
    m_foreign_key_proxy_fields[field->get_name()] = field;
}

Model::~Model() {}

std::time_t Model::get_last_modified_time() const
{
    return m_last_modified_time.get_value();
}

std::string Model::get_runtime_type() const
{
    return m_type.get_value();
}

std::time_t Model::get_creation_time() const
{
    return m_creation_time.get_value();
}

void Model::init_creation_time(std::time_t ctime)
{
    m_creation_time.init_value(ctime);
}

void Model::init_modification_time(std::time_t mtime)
{
    m_last_modified_time.init_value(mtime);
}

const std::string& Model::name() const
{
    return m_name.get_value();
}

void Model::set_name(const std::string& name)
{
    m_name.update_value(name);
}

void Model::set_type(const std::string& type)
{
    m_type.update_value(type);
}

bool Model::valid() const
{
    return !m_id.get_value().empty();
}

void Model::get_foreign_key_fields(VecKeyValuePair& fields) const
{
    for (const auto& [name, dict] : m_named_foreign_key_fields) {
        if (!dict->get_id().empty()) {
            fields.push_back({dict->get_runtime_type(), dict->get_id()});
        }
    }
}

void Model::get_foreign_key_proxy_fields(VecKeyValuePair& fields) const
{
    for (const auto& [name, dict] : m_foreign_key_proxy_fields) {
        fields.push_back({dict->get_runtime_type(), dict->get_name()});
    }
}

}  // namespace hestia