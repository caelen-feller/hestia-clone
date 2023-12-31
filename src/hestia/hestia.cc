#include "hestia.h"

#include "hestia_private.h"

#include "HestiaClient.h"
#include "HestiaServer.h"

#include "HsmItem.h"
#include "JsonUtils.h"
#include "Logger.h"
#include "Stream.h"
#include "StringUtils.h"
#include "UuidUtils.h"

#include "FileStreamSink.h"
#include "FileStreamSource.h"
#include "InMemoryStreamSink.h"
#include "InMemoryStreamSource.h"
#include "ReadableBufferView.h"
#include "WriteableBufferView.h"

#include <iostream>
#include <thread>

namespace hestia {

static std::unique_ptr<IHestiaClient> g_client;
static std::unique_ptr<HestiaServer> g_server;

void HestiaPrivate::override_client(std::unique_ptr<IHestiaClient> client)
{
    g_client = std::move(client);
}

#define ID_AND_STATE_CHECK(oid)                                                \
    if (oid == nullptr) {                                                      \
        return hestia_error_e::HESTIA_ERROR_BAD_INPUT_ID;                      \
    }                                                                          \
    if (!check_initialized()) {                                                \
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;                      \
    }

HestiaType to_subject(hestia_item_t subject)
{
    switch (subject) {
        case hestia_item_e::HESTIA_OBJECT:
            return HestiaType(HsmItem::Type::OBJECT);
        case hestia_item_e::HESTIA_TIER:
            return HestiaType(HsmItem::Type::TIER);
        case hestia_item_e::HESTIA_DATASET:
            return HestiaType(HsmItem::Type::DATASET);
        case hestia_item_e::HESTIA_ACTION:
            return HestiaType(HsmItem::Type::ACTION);
        case hestia_item_e::HESTIA_USER_METADATA:
            return HestiaType(HsmItem::Type::METADATA);
        case hestia_item_e::HESTIA_USER:
            return HestiaType(HestiaType::SystemType::USER);
        case hestia_item_e::HESTIA_NODE:
            return HestiaType(HestiaType::SystemType::HSM_NODE);
        case hestia_item_e::HESTIA_ITEM_TYPE_COUNT:
            return HsmItem::Type::UNKNOWN;
        default:
            return HsmItem::Type::UNKNOWN;
    }
}

CrudIdentifier::InputFormat to_crud_id_format(hestia_id_format_t id_format)
{
    if (id_format == hestia_id_format_t::HESTIA_ID_NONE) {
        return CrudIdentifier::InputFormat::NONE;
    }
    else if (id_format == hestia_id_format_t::HESTIA_ID) {
        return CrudIdentifier::InputFormat::ID;
    }
    else if (id_format == hestia_id_format_t::HESTIA_NAME) {
        return CrudIdentifier::InputFormat::NAME;
    }
    else if (
        id_format
        == (hestia_id_format_t::HESTIA_ID
            | hestia_id_format_t::HESTIA_PARENT_ID)) {
        return CrudIdentifier::InputFormat::ID_PARENT_ID;
    }
    else if (
        id_format
        == (hestia_id_format_t::HESTIA_ID
            | hestia_id_format_t::HESTIA_PARENT_NAME)) {
        return CrudIdentifier::InputFormat::ID_PARENT_NAME;
    }
    else if (
        id_format
        == (hestia_id_format_t::HESTIA_NAME
            | hestia_id_format_t::HESTIA_PARENT_NAME)) {
        return CrudIdentifier::InputFormat::NAME_PARENT_NAME;
    }
    else if (
        id_format
        == (hestia_id_format_t::HESTIA_NAME
            | hestia_id_format_t::HESTIA_PARENT_ID)) {
        return CrudIdentifier::InputFormat::NAME_PARENT_ID;
    }
    else if (id_format == hestia_id_format_t::HESTIA_PARENT_ID) {
        return CrudIdentifier::InputFormat::PARENT_ID;
    }
    return CrudIdentifier::InputFormat::NONE;
}

CrudAttributes::Format to_crud_attr_format(hestia_io_format_t io_format)
{
    if ((io_format & hestia_io_format_t::HESTIA_IO_JSON) != 0) {
        return CrudAttributes::Format::JSON;
    }
    else if ((io_format & hestia_io_format_t::HESTIA_IO_KEY_VALUE) != 0) {
        return CrudAttributes::Format::KEY_VALUE;
    }
    else {
        return CrudAttributes::Format::NONE;
    }
}

extern "C" {

int hestia_start_server(const char* host, int port, const char* config)
{
    if (g_server) {
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    const std::string config_str =
        config == nullptr ? std::string() : std::string(config);

    const std::string host_str = host == nullptr ? std::string() : host;

    Dictionary config_dict;
    if (!config_str.empty()) {
        try {
            JsonUtils::from_json(config_str, config_dict);
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to parse extra_config json in server init(): "
                      << e.what() << "\n"
                      << config_str << std::endl;
            return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
        }
    }

    try {
        g_server = std::make_unique<HestiaServer>();
        g_server->initialize("", "", config_dict, host_str, port);
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize Hestia Server: " << e.what()
                  << std::endl;
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    OpStatus status;
    try {
        status = g_server->run();
    }
    catch (const std::exception& e) {
        status = OpStatus(OpStatus::Status::ERROR, -1, e.what());
    }
    catch (...) {
        status = OpStatus(
            OpStatus::Status::ERROR, -1, "Unknown exception running server.");
    }
    return status.ok() ? 0 : -1;
}

int hestia_stop_server()
{
    if (!g_server) {
        std::cerr
            << "Called finish() but no server is set. Possibly missing initialize() call.";
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    try {
        g_server.reset();
    }
    catch (const std::exception& e) {
        std::cerr << "Error destorying Hestia server: " << e.what()
                  << std::endl;
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }
    return 0;
}

int hestia_initialize(
    const char* config_path, const char* token, const char* extra_config)
{
    if (g_client) {
        g_client->set_last_error(
            "Attempted to initialize client while already running. Call 'finish()' first. ");
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    const std::string config_path_str =
        config_path == nullptr ? std::string() : config_path;
    const std::string token_str = token == nullptr ? std::string() : token;
    const std::string extra_config_str =
        extra_config == nullptr ? std::string() : std::string(extra_config);

    Dictionary extra_config_dict;
    if (!extra_config_str.empty()) {
        try {
            JsonUtils::from_json(extra_config_str, extra_config_dict);
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to parse extra_config json in client init(): "
                      << e.what() << "\n"
                      << extra_config_str << std::endl;
            return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
        }
    }

    try {
        g_client = std::make_unique<HestiaClient>();
        g_client->initialize(config_path_str, token_str, extra_config_dict);
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize Hestia Client: " << e.what()
                  << std::endl;
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }
    return 0;
}

int hestia_finish()
{
    if (!g_client) {
        std::cerr
            << "Called finish() but no client is set. Possibly missing initialize() call.";
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    try {
        g_client.reset();
    }
    catch (const std::exception& e) {
        std::cerr << "Error destorying Hestia client: " << e.what()
                  << std::endl;
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }
    return 0;
}

bool check_initialized()
{
    if (!g_client) {
        std::cerr
            << "Hestia client not initialized. Call 'hestia_initilaize()' first.";
        return false;
    }
    return true;
}

void str_to_char(const std::string& str, char** chars)
{
    *chars = new char[str.size() + 1];
    for (std::size_t idx = 0; idx < str.size(); idx++) {
        (*chars)[idx] = str[idx];
    }
    (*chars)[str.size()] = '\0';
}

int process_results(
    hestia_io_format_t output_format,
    const VecCrudIdentifier& ids,
    const CrudAttributes& attrs,
    char** response,
    int* len_response)
{
    if (output_format == HESTIA_IO_NONE) {
        len_response = 0;
        return 0;
    }

    std::string response_body;
    if ((output_format & HESTIA_IO_IDS) != 0) {
        std::size_t count{0};
        for (const auto& id : ids) {
            response_body += id.get_primary_key();
            if (count < ids.size() - 1) {
                response_body += '\n';
            }
            count++;
        }
    }

    if ((output_format & HESTIA_IO_JSON) != 0
        || (output_format & HESTIA_IO_KEY_VALUE) != 0) {
        if (!response_body.empty()) {
            response_body += '\n';
        }
        response_body += attrs.get_buffer();
    }

    if (!response_body.empty()) {
        str_to_char(response_body, response);
        *len_response = response_body.size();
    }
    else {
        *len_response = 0;
    }
    return 0;
}

int create_or_update(
    hestia_item_t subject,
    hestia_io_format_t input_format,
    hestia_id_format_t id_format,
    const char* input,
    int len_input,
    hestia_io_format_t output_format,
    char** response,
    int* len_response,
    bool is_create)
{
    if (!check_initialized()) {
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    std::vector<CrudIdentifier> ids;
    CrudAttributes attributes;

    const auto crud_id_format     = to_crud_id_format(id_format);
    const auto crud_attr_format   = to_crud_attr_format(input_format);
    const auto crud_output_format = output_format == HESTIA_IO_KEY_VALUE ?
                                        CrudAttributes::Format::KEY_VALUE :
                                        CrudAttributes::Format::JSON;

    if (input_format != HESTIA_IO_NONE) {
        if (input == nullptr) {
            return hestia_error_e::HESTIA_ERROR_BAD_INPUT_ID;
        }

        std::string body(input, len_input);
        std::size_t offset = 0;
        if ((input_format & HESTIA_IO_IDS) != 0
            && crud_id_format != CrudIdentifier::InputFormat::NONE) {
            offset = CrudIdentifier::parse(body, crud_id_format, ids);
        }

        if ((input_format & HESTIA_IO_JSON) != 0
            || (input_format & HESTIA_IO_KEY_VALUE) != 0) {
            attributes.set_buffer(
                body.substr(offset, body.size() - offset), crud_attr_format);
        }
    }

    if (is_create) {
        if (const auto status = g_client->create(
                to_subject(subject), ids, attributes, crud_output_format);
            !status.ok()) {
            return status.m_error_code;
        }
    }
    else {
        if (const auto status = g_client->update(
                to_subject(subject), ids, attributes, crud_output_format);
            !status.ok()) {
            return status.m_error_code;
        }
    }

    return process_results(
        output_format, ids, attributes, response, len_response);
}

int hestia_create(
    hestia_item_t subject,
    hestia_io_format_t input_format,
    hestia_id_format_t id_format,
    const char* input,
    int len_input,
    hestia_io_format_t output_format,
    char** response,
    int* len_response)
{
    return create_or_update(
        subject, input_format, id_format, input, len_input, output_format,
        response, len_response, true);
}

int hestia_free_output(char** output)
{
    delete[] * output;
    return 0;
}

int hestia_update(
    hestia_item_t subject,
    hestia_io_format_t input_format,
    hestia_id_format_t id_format,
    const char* input,
    int len_input,
    hestia_io_format_t output_format,
    char** response,
    int* len_response)
{
    return create_or_update(
        subject, input_format, id_format, input, len_input, output_format,
        response, len_response, false);
}

int hestia_identify(
    hestia_item_t subject,
    hestia_id_format_t id_format,
    const char* input,
    int len_input,
    const char* output,
    int* len_output,
    int* exists)
{
    (void)subject;
    (void)id_format;
    (void)input;
    (void)len_input;
    (void)output;
    (void)len_output;
    (void)exists;
    return -1;
}

int hestia_read(
    hestia_item_t subject,
    hestia_query_format_t query_format,
    hestia_id_format_t id_format,
    int offset,
    int count,
    const char* query,
    int len_query,
    hestia_io_format_t output_format,
    char** response,
    int* len_response,
    int* total_count)
{
    if (!check_initialized()) {
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    (void)total_count;

    std::string query_body;
    if (query_format != HESTIA_QUERY_NONE) {
        if (query == nullptr) {
            return hestia_error_e::HESTIA_ERROR_BAD_INPUT_ID;
        }
        query_body = std::string(query, len_query);
    }

    CrudQuery crud_query;
    if (query_format == HESTIA_QUERY_IDS) {
        std::vector<CrudIdentifier> ids;
        const auto crud_id_format = to_crud_id_format(id_format);
        CrudIdentifier::parse(query_body, crud_id_format, ids);
        crud_query.set_ids(ids);
    }
    else if (query_format == HESTIA_QUERY_FILTER) {
        Map filter;
        auto [key, value] = StringUtils::split_on_first(query_body, ',');
        StringUtils::trim(key);
        StringUtils::trim(value);
        filter.set_item(key, value);
        crud_query.set_filter(filter);
    }

    crud_query.set_offset(offset);
    crud_query.set_count(count);

    const auto crud_attrs_output_format =
        (output_format & HESTIA_IO_KEY_VALUE) != 0 ?
            CrudAttributes::Format::KEY_VALUE :
            CrudAttributes::Format::JSON;
    crud_query.set_attributes_output_format(crud_attrs_output_format);

    CrudQuery::OutputFormat crud_output_format{CrudQuery::OutputFormat::ID};
    if (output_format != HESTIA_IO_IDS) {
        crud_output_format = CrudQuery::OutputFormat::ATTRIBUTES;
    }

    crud_query.set_output_format(crud_output_format);

    const auto status = g_client->read(to_subject(subject), crud_query);
    if (!status.ok()) {
        return status.m_error_code;
    }

    return process_results(
        output_format, crud_query.ids(), crud_query.get_attributes(), response,
        len_response);
}

int hestia_remove(
    hestia_item_t subject,
    hestia_id_format_t id_format,
    const char* input,
    int len_input)
{
    if (!check_initialized()) {
        return hestia_error_e::HESTIA_ERROR_CLIENT_STATE;
    }

    if (input == nullptr) {
        return hestia_error_e::HESTIA_ERROR_BAD_INPUT_ID;
    }

    std::vector<CrudIdentifier> ids;
    const auto crud_id_format = to_crud_id_format(id_format);

    std::string body(input, len_input);
    CrudIdentifier::parse(body, crud_id_format, ids);

    const auto status = g_client->remove(to_subject(subject), ids);
    return status.m_error_code;
}

int hestia_data_put(
    const char* oid,
    const void* buf,
    const size_t length,
    const size_t offset,
    const uint8_t target_tier,
    char** activity_id,
    int* len_activity_id)
{
    if (buf == nullptr) {
        return hestia_error_e::HESTIA_ERROR_BAD_INPUT_BUFFER;
    }

    ID_AND_STATE_CHECK(oid);

    hestia::Stream stream;
    stream.set_source(hestia::InMemoryStreamSource::create(
        hestia::ReadableBufferView(buf, length)));

    HsmAction action(HsmItem::Type::OBJECT, HsmAction::Action::PUT_DATA);
    action.set_offset(offset);
    action.set_target_tier(target_tier);
    action.set_subject_key(std::string(oid));

    OpStatus status;
    auto completion_cb = [&status, activity_id, len_activity_id](
                             OpStatus ret_status, const HsmAction& action) {
        status = ret_status;

        LOG_INFO("Put action completed");
        if (ret_status.ok()) {
            str_to_char(action.get_primary_key(), activity_id);
            *len_activity_id = action.get_primary_key().size();
        }
        else {
            *len_activity_id = 0;
        }
    };

    g_client->do_data_io_action(action, &stream, completion_cb);

    if (stream.waiting_for_content()) {
        auto stream_status = stream.flush();
        if (!stream_status.ok()) {
            return hestia_error_e::HESTIA_ERROR_BAD_STREAM;
        }
    }
    else {
        auto stream_status = stream.reset();
        if (!stream_status.ok()) {
            return hestia_error_e::HESTIA_ERROR_BAD_STREAM;
        }
    }

    return status.m_error_code;
}

int hestia_data_put_path(
    const char* oid,
    const char* path,
    const size_t length,
    const size_t offset,
    const uint8_t tier,
    char** activity_id,
    int* len_activity_id)
{
    if (path == nullptr) {
        return hestia_error_e::HESTIA_ERROR_BAD_INPUT_BUFFER;
    }

    ID_AND_STATE_CHECK(oid);

    hestia::Stream stream;
    stream.set_source(FileStreamSource::create(path));

    HsmAction action(HsmItem::Type::OBJECT, HsmAction::Action::PUT_DATA);
    action.set_offset(offset);
    action.set_target_tier(tier);
    action.set_subject_key(std::string(oid));

    if (length == 0) {
        action.set_size(stream.get_source_size());
    }
    else {
        action.set_size(length);
    }

    OpStatus status;
    auto completion_cb = [&status, activity_id, len_activity_id](
                             OpStatus ret_status, const HsmAction& action) {
        status = ret_status;

        if (ret_status.ok()) {
            str_to_char(action.get_primary_key(), activity_id);
            *len_activity_id = action.get_primary_key().size();
        }
        else {
            *len_activity_id = 0;
        }
    };

    g_client->do_data_io_action(action, &stream, completion_cb);

    auto stream_status = stream.flush();
    if (!stream_status.ok()) {
        return hestia_error_e::HESTIA_ERROR_BAD_STREAM;
    }

    return status.m_error_code;
}

int hestia_data_put_descriptor(
    const char* oid,
    int fd,
    const size_t length,
    const size_t offset,
    const uint8_t target_tier,
    char** activity_id,
    int* len_activity_id)
{
    ID_AND_STATE_CHECK(oid);

    hestia::Stream stream;
    stream.set_source(FileStreamSource::create(fd, length));

    HsmAction action(HsmItem::Type::OBJECT, HsmAction::Action::PUT_DATA);
    action.set_offset(offset);
    action.set_target_tier(target_tier);
    action.set_subject_key(oid);
    action.set_size(length);

    OpStatus status;
    auto completion_cb = [&status, activity_id, len_activity_id](
                             OpStatus ret_status, const HsmAction& action) {
        status = ret_status;

        if (ret_status.ok()) {
            str_to_char(action.get_primary_key(), activity_id);
            *len_activity_id = action.get_primary_key().size();
        }
        else {
            *len_activity_id = 0;
        }
    };

    g_client->do_data_io_action(action, &stream, completion_cb);

    auto stream_status = stream.flush();
    if (!stream_status.ok()) {
        return hestia_error_e::HESTIA_ERROR_BAD_STREAM;
    }

    return status.m_error_code;
}

int hestia_data_get(
    const char* oid,
    void* buf,
    size_t* length,
    const size_t offset,
    const uint8_t src_tier,
    char** activity_id,
    int* len_activity_id)
{
    if (buf == nullptr) {
        return hestia_error_e::HESTIA_ERROR_BAD_INPUT_BUFFER;
    }

    if (length == nullptr) {
        return hestia_error_e::HESTIA_ERROR_BAD_INPUT_BUFFER;
    }

    ID_AND_STATE_CHECK(oid);

    hestia::WriteableBufferView writeable_buffer(buf, *length);
    hestia::Stream stream;
    stream.set_sink(hestia::InMemoryStreamSink::create(writeable_buffer));

    HsmAction action(HsmItem::Type::OBJECT, HsmAction::Action::GET_DATA);
    action.set_offset(offset);
    action.set_source_tier(src_tier);
    action.set_subject_key(oid);
    action.set_size(*length);

    OpStatus status;
    auto completion_cb = [&status, activity_id, len_activity_id](
                             OpStatus ret_status, const HsmAction& action) {
        status = ret_status;

        if (ret_status.ok()) {
            str_to_char(action.get_primary_key(), activity_id);
            *len_activity_id = action.get_primary_key().size();
        }
        else {
            *len_activity_id = 0;
        }
    };

    g_client->do_data_io_action(action, &stream, completion_cb);

    if (stream.has_source()) {
        auto stream_status = stream.flush();
        if (!stream_status.ok()) {
            return hestia_error_e::HESTIA_ERROR_BAD_STREAM;
        }
        *length = stream_status.get_num_transferred();
    }
    else {
        *length = stream.get_num_transferred();
    }
    return status.m_error_code;
}

int hestia_data_get_descriptor(
    const char* oid,
    int file_discriptor,
    const size_t len,
    const size_t offset,
    const uint8_t tier,
    char** activity_id,
    int* len_activity_id)
{
    ID_AND_STATE_CHECK(oid);

    hestia::Stream stream;
    stream.set_sink(FileStreamSink::create(file_discriptor, len));

    HsmAction action(HsmItem::Type::OBJECT, HsmAction::Action::GET_DATA);
    action.set_offset(offset);
    action.set_source_tier(tier);
    action.set_subject_key(oid);
    action.set_size(len);

    OpStatus status;
    auto completion_cb = [&status, activity_id, len_activity_id](
                             OpStatus ret_status, const HsmAction& action) {
        status = ret_status;

        if (ret_status.ok()) {
            str_to_char(action.get_primary_key(), activity_id);
            *len_activity_id = action.get_primary_key().size();
        }
        else {
            *len_activity_id = 0;
        }
    };

    g_client->do_data_io_action(action, &stream, completion_cb);

    auto stream_status = stream.flush();
    if (!stream_status.ok()) {
        return hestia_error_e::HESTIA_ERROR_BAD_STREAM;
    }
    return status.m_error_code;
}

int hestia_data_get_path(
    const char* oid,
    const char* path,
    const size_t len,
    const size_t offset,
    const uint8_t tier,
    char** activity_id,
    int* len_activity_id)
{
    ID_AND_STATE_CHECK(oid);

    hestia::Stream stream;
    stream.set_sink(FileStreamSink::create(path));

    HsmAction action(HsmItem::Type::OBJECT, HsmAction::Action::GET_DATA);
    action.set_offset(offset);
    action.set_source_tier(tier);
    action.set_subject_key(oid);

    if (len > 0) {
        action.set_size(len);
    }

    OpStatus status;
    auto completion_cb = [&status, activity_id, len_activity_id](
                             OpStatus ret_status, const HsmAction& action) {
        status = ret_status;
        LOG_INFO("Completion cb fired");

        if (ret_status.ok()) {
            str_to_char(action.get_primary_key(), activity_id);
            *len_activity_id = action.get_primary_key().size();
        }
        else {
            *len_activity_id = 0;
        }
    };

    g_client->do_data_io_action(action, &stream, completion_cb);

    auto stream_status = stream.flush();
    if (!stream_status.ok()) {
        return hestia_error_e::HESTIA_ERROR_BAD_STREAM;
    }
    return status.m_error_code;
}

int hestia_data_copy(
    hestia_item_t subject,
    const char* id,
    const uint8_t src_tier,
    const uint8_t tgt_tier,
    char** activity_id,
    int* len_activity_id)
{
    ID_AND_STATE_CHECK(id);

    HsmAction action(
        to_subject(subject).m_hsm_type, HsmAction::Action::COPY_DATA);
    action.set_source_tier(src_tier);
    action.set_target_tier(tgt_tier);
    action.set_subject_key(id);

    const auto status = g_client->do_data_movement_action(action);
    if (status.ok()) {
        str_to_char(action.get_primary_key(), activity_id);
        *len_activity_id = action.get_primary_key().size();
    }

    return status.m_error_code;
}

int hestia_data_move(
    hestia_item_t subject,
    const char* id,
    const std::uint8_t src_tier,
    const std::uint8_t tgt_tier,
    char** activity_id,
    int* len_activity_id)
{
    ID_AND_STATE_CHECK(id);

    HsmAction action(
        to_subject(subject).m_hsm_type, HsmAction::Action::MOVE_DATA);
    action.set_source_tier(src_tier);
    action.set_target_tier(tgt_tier);
    action.set_subject_key(id);

    const auto status = g_client->do_data_movement_action(action);
    if (status.ok()) {
        str_to_char(action.get_primary_key(), activity_id);
        *len_activity_id = action.get_primary_key().size();
    }
    return status.m_error_code;
}

int hestia_data_release(
    hestia_item_t subject,
    const char* id,
    const uint8_t src_tier,
    char** activity_id,
    int* len_activity_id)
{
    ID_AND_STATE_CHECK(id);

    HsmAction action(
        to_subject(subject).m_hsm_type, HsmAction::Action::RELEASE_DATA);
    action.set_source_tier(src_tier);
    action.set_subject_key(id);

    const auto status = g_client->do_data_movement_action(action);
    if (status.ok()) {
        str_to_char(action.get_primary_key(), activity_id);
        *len_activity_id = action.get_primary_key().size();
    }
    return status.m_error_code;
}
}
}  // namespace hestia