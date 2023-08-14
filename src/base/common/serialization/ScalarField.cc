#include "ScalarField.h"

#include "TimeUtils.h"
#include "UuidUtils.h"

namespace hestia {
ScalarField::ScalarField(const std::string& name, IndexScope index_scope) :
    BaseField(name, index_scope)
{
}

StringField::StringField(
    const std::string& name, const std::string& default_value) :
    TypedScalarField<std::string>(name, default_value)
{
}

std::string StringField::value_as_string() const
{
    return m_value;
}

void StringField::value_from_string(const std::string& value_str)
{
    update_value(value_str);
}

DateTimeField::DateTimeField(
    const std::string& name, std::time_t default_value) :
    TypedScalarField<std::time_t>(name, default_value)
{
}

std::string DateTimeField::value_as_string() const
{
    return std::to_string(m_value);
}

void DateTimeField::value_from_string(const std::string& value_str)
{
    update_value(std::stoull(value_str));
}

void DateTimeField::set_as_now()
{
    update_value(TimeUtils::get_current_time());
}

UuidField::UuidField(const std::string& name, const Uuid& default_value) :
    TypedScalarField<Uuid>(name, default_value)
{
}

std::string UuidField::value_as_string() const
{
    return UuidUtils::to_string(m_value);
}

void UuidField::value_from_string(const std::string& value_str)
{
    update_value(UuidUtils::from_string(value_str));
}

BooleanField::BooleanField(const std::string& name, bool default_value) :
    TypedScalarField<bool>(name, default_value)
{
}

std::string BooleanField::value_as_string() const
{
    return m_value ? "true" : "false";
}

void BooleanField::value_from_string(const std::string& value_str)
{
    update_value(value_str == "true");
}

UIntegerField::UIntegerField(
    const std::string& name, unsigned long default_value) :
    TypedScalarField<unsigned long>(name, default_value)
{
}

std::string UIntegerField::value_as_string() const
{
    return std::to_string(m_value);
}

void UIntegerField::value_from_string(const std::string& value_str)
{
    update_value(std::stoul(value_str));
}
}  // namespace hestia