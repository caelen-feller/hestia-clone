#include "HestiaClient.h"

#include "HestiaConfig.h"
#include "hestia.h"

#include "DistributedHsmService.h"
#include "HsmService.h"
#include "HsmServicesFactory.h"
#include "UserService.h"

#include "ErrorUtils.h"
#include "FileHsmObjectStoreClient.h"
#include "HsmObjectStoreClient.h"
#include "HttpClient.h"
#include "JsonUtils.h"

#include "Logger.h"

#include <iostream>
#include <stdexcept>

namespace hestia {

namespace rc {
OpStatus error(const RequestError<HsmActionErrorCode>& error)
{
    switch (error.code()) {
        case HsmActionErrorCode::NO_ERROR:
            return {};
        case HsmActionErrorCode::ERROR:
        case HsmActionErrorCode::STL_EXCEPTION:
        case HsmActionErrorCode::UNKNOWN_EXCEPTION:
        case HsmActionErrorCode::UNSUPPORTED_REQUEST_METHOD:
        case HsmActionErrorCode::ITEM_NOT_FOUND:
        case HsmActionErrorCode::BAD_PUT_OVERWRITE_COMBINATION:
        case HsmActionErrorCode::MAX_ERROR:
        default:
            return {
                OpStatus::Status::ERROR, hestia_error_t::HESTIA_ERROR_UNKNOWN,
                error.message()};
    }
}

OpStatus error(const RequestError<CrudErrorCode>& error)
{
    switch (error.code()) {
        case CrudErrorCode::NO_ERROR:
            return {};
        case CrudErrorCode::ERROR:
        case CrudErrorCode::STL_EXCEPTION:
        case CrudErrorCode::NOT_AUTHENTICATED:
        case CrudErrorCode::UNKNOWN_EXCEPTION:
        case CrudErrorCode::UNSUPPORTED_REQUEST_METHOD:
        case CrudErrorCode::ITEM_NOT_FOUND:
        case CrudErrorCode::CANT_OVERRIDE_EXISTING:
        case CrudErrorCode::MAX_ERROR:
        default:
            return {
                OpStatus::Status::ERROR, hestia_error_t::HESTIA_ERROR_UNKNOWN,
                error.message()};
    }
}

OpStatus bad_stream()
{
    return {
        OpStatus::Status::ERROR, hestia_error_t::HESTIA_ERROR_BAD_STREAM,
        "Stream failed"};
}

}  // namespace rc

#define ERROR_CHECK(response, method_str)                                      \
    if (!response->ok()) {                                                     \
        set_last_error(                                                        \
            "Error in " + std::string(method_str) + ": "                       \
            + response->get_error().to_string());                              \
        return rc::error(response->get_error());                               \
    }

HestiaClient::HestiaClient() : HestiaApplication() {}

HestiaClient::~HestiaClient() {}

OpStatus HestiaClient::initialize(
    const std::string& config_path,
    const std::string& user_token,
    const Dictionary& extra_config,
    const std::string& server_host,
    unsigned server_port)
{
    try {
        HestiaApplication::initialize(
            config_path, user_token, extra_config, server_host, server_port);
    }
    catch (const std::exception& e) {
        set_last_error(e.what());
        return {
            OpStatus::Status::ERROR, hestia_error_t::HESTIA_ERROR_CLIENT_STATE,
            SOURCE_LOC() + " | Failed to initialize Hestia Application.\n"
                + e.what()};
    }
    return {};
}

void HestiaClient::clear_last_error()
{
    std::scoped_lock guard(m_mutex);
    m_last_errors[std::this_thread::get_id()].clear();
}

void HestiaClient::set_last_error(const std::string& msg)
{
    LOG_ERROR(msg);
    {
        std::scoped_lock guard(m_mutex);
        m_last_errors[std::this_thread::get_id()] = msg;
    }
}

void HestiaClient::get_last_error(std::string& error)
{
    std::scoped_lock guard(m_mutex);
    error = m_last_errors[std::this_thread::get_id()];
}

OpStatus HestiaClient::create(
    const HestiaType& subject,
    VecCrudIdentifier& ids,
    CrudAttributes& attributes,
    CrudAttributes::Format output_format)
{
    clear_last_error();

    CrudService* service{nullptr};
    if (subject.m_type == HestiaType::Type::SYSTEM
        && subject.m_system_type == HestiaType::SystemType::USER) {
        service = m_user_service.get();
    }
    else if (subject.m_type == HestiaType::Type::HSM) {
        service = m_hsm_service;
    }
    if (service != nullptr) {
        const auto response = service->make_request(
            CrudRequest{
                CrudMethod::CREATE, m_user_service->get_current_user_context(),
                ids, attributes, CrudQuery::OutputFormat::ATTRIBUTES,
                output_format},
            HsmItem::to_name(subject.m_hsm_type));
        ERROR_CHECK(response, "CREATE");
        ids.clear();
        for (const auto& id : response->ids()) {
            ids.push_back(
                CrudIdentifier(id, CrudIdentifier::Type::PRIMARY_KEY));
        }
        attributes = response->attributes();
    }
    return {};
}

OpStatus HestiaClient::update(
    const HestiaType& subject,
    const VecCrudIdentifier& ids,
    CrudAttributes& attributes,
    CrudAttributes::Format output_format)
{
    clear_last_error();

    CrudService* service{nullptr};
    if (subject.m_type == HestiaType::Type::SYSTEM
        && subject.m_system_type == HestiaType::SystemType::USER) {
        service = m_user_service.get();
    }
    else if (subject.m_type == HestiaType::Type::HSM) {
        service = m_hsm_service;
    }
    if (service != nullptr) {
        if (ids.size() == 1) {
            LOG_INFO("Doing update with id: " << ids[0].get_primary_key());
        }
        else {
            LOG_INFO("Doing update with: " << ids.size() << " ids.");
        }
        LOG_INFO(
            "Attribute format is: " << attributes.get_input_format_as_string());

        const auto response = service->make_request(
            CrudRequest{
                CrudMethod::UPDATE, m_user_service->get_current_user_context(),
                ids, attributes, CrudQuery::OutputFormat::ATTRIBUTES,
                output_format},
            HsmItem::to_name(subject.m_hsm_type));
        ERROR_CHECK(response, "UPDATE");
        attributes = response->attributes();
    }
    return {};
}

OpStatus HestiaClient::remove(
    const HestiaType& subject, const VecCrudIdentifier& ids)
{
    clear_last_error();

    CrudService* service{nullptr};
    if (subject.m_type == HestiaType::Type::SYSTEM
        && subject.m_system_type == HestiaType::SystemType::USER) {
        service = m_user_service.get();
    }
    else if (subject.m_type == HestiaType::Type::HSM) {
        service = m_hsm_service;
    }
    if (service != nullptr) {

        if (ids.size() == 1) {
            LOG_INFO("Doing remove with id: " << ids[0].get_primary_key());
        }
        else {
            LOG_INFO("Doing remove with: " << ids.size() << " ids.");
        }

        const auto response = service->make_request(
            CrudRequest{
                CrudMethod::REMOVE, m_user_service->get_current_user_context(),
                ids},
            HsmItem::to_name(subject.m_hsm_type));
        ERROR_CHECK(response, "REMOVE");
    }
    return {};
}

OpStatus HestiaClient::read(const HestiaType& subject, CrudQuery& query)
{
    clear_last_error();

    // Serializeable::Format format = Serializeable::Format::CHILD_ID;
    CrudService* service{nullptr};
    if (subject.m_type == HestiaType::Type::SYSTEM
        && subject.m_system_type == HestiaType::SystemType::USER) {
        service = m_user_service.get();
    }
    else {
        service = m_hsm_service;
    }

    if (service != nullptr) {
        if (query.get_format() == CrudQuery::Format::ID) {
            if (query.get_ids().size() == 1) {
                LOG_INFO(
                    "Doing query with id: "
                    << query.get_ids()[0].get_primary_key());
            }
            else {
                LOG_INFO(
                    "Doing query with: " << query.get_ids().size() << " ids.");
            }
        }

        CrudResponsePtr response;
        response = service->make_request(
            CrudRequest{query, m_user_service->get_current_user_context()},
            HsmItem::to_name(subject.m_hsm_type));
        ERROR_CHECK(response, "READ");
        query.attributes() = response->attributes();

        VecCrudIdentifier ids;
        for (const auto& id : response->ids()) {
            ids.push_back(id);
        }
        query.set_ids(ids);
    }
    return {};
}

std::string HestiaClient::get_runtime_info() const
{
    return HestiaApplication::get_runtime_info();
}

void HestiaClient::do_data_io_action(
    const HsmAction& action,
    Stream* stream,
    dataIoCompletionFunc completion_func)
{
    clear_last_error();

    auto hsm_completion_func =
        [this, completion_func](HsmActionResponse::Ptr response) {
            if (!response->ok()) {
                set_last_error(
                    "Error in data io operation: "
                    + response->get_error().to_string());
                completion_func(
                    rc::error(response->get_error()), response->get_action());
            }
            else {
                completion_func({}, response->get_action());
            }
        };
    m_hsm_service->do_data_io_action(
        HsmActionRequest{action, m_user_service->get_current_user_context()},
        stream, hsm_completion_func);
}

OpStatus HestiaClient::do_data_movement_action(HsmAction& action)
{
    clear_last_error();

    if (const auto response = m_hsm_service->make_request(
            {action, m_user_service->get_current_user_context()});
        !response->ok()) {
        set_last_error(
            "Error in data movement operation: "
            + response->get_error().to_string());
        return rc::error(response->get_error());
    }
    else {
        action = response->get_action();
    }
    return {};
}

void HestiaClient::load_object_store_defaults()
{
    LOG_INFO("Loading fallback file based object store and tiers");
    ObjectStoreBackend backend(ObjectStoreBackend::Type::FILE_HSM);

    std::vector<std::string> tier_names;
    for (uint8_t idx = 0; idx < 5; idx++) {

        StorageTier tier(idx);
        tier_names.push_back(std::to_string(idx));
        m_config.add_storage_tier(tier);
    }
    backend.set_tier_names(tier_names);
    m_config.add_object_store_backend(backend);
}

void HestiaClient::set_app_mode(const std::string& host, unsigned port)
{
    if (!host.empty()) {
        m_config.override_controller_address(host, port);
    }

    if (!m_config.get_server_config().has_controller_address()) {
        LOG_INFO("Running CLIENT STANDALONE mode");
        m_app_mode = ApplicationMode::CLIENT_STANDALONE;
        if (!m_config.has_object_store_backends()) {
            LOG_INFO(
                "No object stores found in Standalone mode - adding a demo backend.");
            load_object_store_defaults();
        }
    }
    else {
        LOG_INFO("Running CLIENT FULL mode");
        m_app_mode = ApplicationMode::CLIENT_FULL;
    }
}

}  // namespace hestia