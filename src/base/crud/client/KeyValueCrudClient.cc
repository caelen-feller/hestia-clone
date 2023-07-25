#include "KeyValueCrudClient.h"

#include "IdGenerator.h"
#include "KeyValueStoreClient.h"
#include "KeyValueStoreRequest.h"
#include "StringAdapter.h"
#include "TimeProvider.h"

#include "Logger.h"
#include "RequestException.h"

#include <cassert>
#include <iostream>

namespace hestia {
KeyValueCrudClient::KeyValueCrudClient(
    const CrudClientConfig& config,
    AdapterCollectionPtr adapters,
    KeyValueStoreClient* client,
    IdGenerator* id_generator,
    TimeProvider* time_provider) :
    CrudClient(config, std::move(adapters), id_generator, time_provider),
    m_client(client)
{
}

KeyValueCrudClient::~KeyValueCrudClient() {}

void KeyValueCrudClient::create(
    const CrudRequest& crud_request, CrudResponse& crud_response) const
{
    std::vector<std::string> ids;
    std::vector<VecKeyValuePair> index_fields;
    std::vector<VecKeyValuePair> foregin_key_fields;

    Dictionary attributes_dict;
    if (crud_request.get_attributes().has_content()) {
        auto adapter = get_adapter(crud_request.get_attributes().get_format());
        adapter->dict_from_string(
            crud_request.get_attributes().get_buffer(), attributes_dict);
    }

    Dictionary content(Dictionary::Type::SEQUENCE);
    auto item_template = m_adapters->get_model_factory()->create();

    if (crud_request.has_items()) {
        std::size_t count{0};
        for (const auto& item : crud_request.items()) {
            auto id = item->get_primary_key();
            if (id.empty()) {
                id = generate_id(item->name());
            }
            ids.push_back(id);

            VecKeyValuePair index;
            item->get_index_fields(index);
            index_fields.push_back(index);

            VecKeyValuePair foregin_keys;
            item->get_foreign_key_fields(foregin_keys);
            foregin_key_fields.push_back(foregin_keys);

            auto item_dict = std::make_unique<Dictionary>();
            get_adapter(CrudAttributes::Format::JSON)
                ->to_dict(crud_request.items(), *item_dict, {}, count);
            content.add_sequence_item(std::move(item_dict));
            count++;
        }
    }
    else if (!crud_request.get_ids().empty()) {

        std::size_t count{0};
        for (const auto& crud_id : crud_request.get_ids()) {
            std::string id = crud_request.get_ids()[0].get_primary_key();
            if (id.empty()) {
                id = generate_id(crud_id.get_name());
            }
            ids.push_back(id);

            auto base_item = m_adapters->get_model_factory()->create();

            VecKeyValuePair index;
            base_item->get_index_fields(index);
            index_fields.push_back(index);

            if (!attributes_dict.is_empty()) {
                if (attributes_dict.get_type() == Dictionary::Type::MAP) {
                    base_item->deserialize(attributes_dict);
                }
                else if (
                    attributes_dict.get_type() == Dictionary::Type::SEQUENCE
                    && attributes_dict.get_sequence().size()
                           == crud_request.get_ids().size()) {
                    base_item->deserialize(
                        *attributes_dict.get_sequence()[count]);
                }
            }

            VecKeyValuePair foregin_keys;
            base_item->get_foreign_key_fields(foregin_keys);
            foregin_key_fields.push_back(foregin_keys);

            VecModelPtr base_items;
            base_items.push_back(std::move(base_item));

            auto item_dict = std::make_unique<Dictionary>();
            get_adapter(CrudAttributes::Format::JSON)
                ->to_dict(base_items, *item_dict, {});
            content.add_sequence_item(std::move(item_dict));
            count++;
        }
    }
    else {
        auto base_item = m_adapters->get_model_factory()->create();

        VecKeyValuePair index;
        base_item->get_index_fields(index);
        index_fields.push_back(index);

        if (!attributes_dict.is_empty()
            && attributes_dict.get_type() == Dictionary::Type::MAP) {
            base_item->deserialize(attributes_dict);
        }

        VecKeyValuePair foregin_keys;
        base_item->get_foreign_key_fields(foregin_keys);
        foregin_key_fields.push_back(foregin_keys);

        VecModelPtr base_items;
        base_items.push_back(std::move(base_item));

        ids.push_back(generate_id(""));
        auto item_dict = std::make_unique<Dictionary>();
        get_adapter(CrudAttributes::Format::JSON)
            ->to_dict(base_items, *item_dict, {});

        content.add_sequence_item(std::move(item_dict));
    }

    const auto current_time = m_time_provider->get_current_time();

    ModelCreationContext create_context(item_template->get_runtime_type());
    create_context.m_creation_time.update_value(current_time);
    create_context.m_last_modified_time.update_value(current_time);

    Dictionary create_context_dict;
    create_context.serialize(create_context_dict);

    assert(ids.size() == content.get_sequence().size());

    std::size_t count{0};
    std::vector<KeyValuePair> string_set_kv_pairs;
    std::vector<KeyValuePair> set_add_kv_pairs;

    for (const auto& id : ids) {
        auto item_dict = content.get_sequence()[count].get();
        item_dict->merge(create_context_dict);

        auto primary_key_dict =
            std::make_unique<Dictionary>(Dictionary::Type::SCALAR);
        primary_key_dict->set_scalar(id);
        item_dict->set_map_item(
            item_template->get_primary_key_name(), std::move(primary_key_dict));

        std::string content_body;
        get_adapter(CrudAttributes::Format::JSON)
            ->dict_to_string(*item_dict, content_body);

        string_set_kv_pairs.emplace_back(get_item_key(id), content_body);

        for (const auto& [name, value] : index_fields[count]) {
            string_set_kv_pairs.emplace_back(get_field_key(name, value), id);
        }

        for (const auto& [type, field_id] : foregin_key_fields[count]) {
            const auto foreign_key = m_config.m_prefix + ":" + type + ":"
                                     + field_id + ":" + m_adapters->get_type()
                                     + "s";
            set_add_kv_pairs.emplace_back(foreign_key, id);
        }

        set_add_kv_pairs.emplace_back(get_set_key(), id);
        count++;
    }

    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_SET, string_set_kv_pairs,
         m_config.m_endpoint});
    error_check("STRING_SET", response.get());

    const auto set_response = m_client->make_request(
        {KeyValueStoreRequestMethod::SET_ADD, set_add_kv_pairs,
         m_config.m_endpoint});
    error_check("SET_ADD", set_response.get());

    if (crud_request.get_query().is_attribute_output_format()) {
        get_adapter(CrudAttributes::Format::JSON)
            ->dict_to_string(content, crud_response.attributes().buffer());
    }
    else {
        get_adapter(CrudAttributes::Format::JSON)
            ->from_dict(content, crud_response.items());
    }
    crud_response.ids() = ids;
}

void KeyValueCrudClient::update(
    const CrudRequest& crud_request, CrudResponse& crud_response) const
{
    std::vector<std::string> ids;
    if (crud_request.has_items()) {
        for (const auto& item : crud_request.items()) {
            ids.push_back(item->get_primary_key());
        }
    }
    else {
        for (const auto& id : crud_request.get_ids()) {
            ids.push_back(id.get_primary_key());
        }
    }

    std::vector<std::string> string_get_keys;
    for (const auto& id : ids) {
        string_get_keys.push_back(get_item_key(id));
    }

    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_GET, string_get_keys,
         m_config.m_endpoint});
    error_check("GET", response.get());

    const auto any_false = std::any_of(
        response->found().begin(), response->found().end(),
        [](bool v) { return !v; });
    if (any_false) {
        throw std::runtime_error("Attempted to update a non-existing resource");
    }

    VecModelPtr db_items;
    get_adapter(CrudAttributes::Format::JSON)
        ->from_string(response->items(), db_items);

    const auto current_time = m_time_provider->get_current_time();
    ModelUpdateContext update_context(m_adapters->get_type());
    update_context.m_last_modified_time.update_value(current_time);
    Dictionary update_context_dict;
    update_context.serialize(update_context_dict);

    std::size_t count = 0;
    if (crud_request.has_items()) {
        for (auto& db_item : db_items) {
            Dictionary modified_dict;
            crud_request.items()[count]->serialize(
                modified_dict, Serializeable::Format::MODIFIED);

            modified_dict.merge(update_context_dict);

            db_item->deserialize(modified_dict);
            count++;
        }
    }

    Dictionary updated_content;
    auto adapter = get_adapter(CrudAttributes::Format::JSON);

    adapter->to_dict(db_items, updated_content);

    std::vector<KeyValuePair> string_set_kv_pairs;
    if (updated_content.get_type() == Dictionary::Type::SEQUENCE) {
        count = 0;
        assert(string_get_keys.size() == updated_content.get_sequence().size());
        for (const auto& dict_item : updated_content.get_sequence()) {
            std::string content;
            adapter->dict_to_string(*dict_item, content);
            string_set_kv_pairs.push_back({string_get_keys[count], content});
            count++;
        }
    }
    else {
        assert(string_get_keys.size() == 1);
        std::string content;
        adapter->dict_to_string(updated_content, content);
        string_set_kv_pairs.push_back({string_get_keys[0], content});
    }

    auto set_response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_SET, string_set_kv_pairs,
         m_config.m_endpoint});
    error_check("STRING_SET", set_response.get());

    if (crud_request.get_query().is_item_output_format()) {
        adapter->from_dict(updated_content, crud_response.items());
    }
}

void KeyValueCrudClient::read(
    const CrudQuery& query, CrudResponse& crud_response) const
{
    std::vector<std::string> string_get_keys;
    std::vector<std::string> foreign_key_proxy_keys;

    auto template_item = m_adapters->get_model_factory()->create();
    VecKeyValuePair foreign_key_proxies;
    template_item->get_foreign_key_proxy_fields(foreign_key_proxies);

    if (query.is_id()) {
        for (const auto& id : query.ids()) {
            std::string working_id = id.get_primary_key();
            if (id.has_primary_key()) {
                string_get_keys.push_back(get_item_key(working_id));
            }
            else if (id.has_name()) {
                const auto id_request_key =
                    get_field_key("name", id.get_name());
                const auto response = m_client->make_request(
                    {KeyValueStoreRequestMethod::STRING_GET,
                     {id_request_key},
                     m_config.m_endpoint});
                error_check("STRING_GET", response.get());
                if (response->items().empty() || response->items()[0].empty()) {
                    return;
                }
                working_id = response->items()[0];
                string_get_keys.push_back(get_item_key(working_id));
            }
            for (const auto& [type, name] : foreign_key_proxies) {
                const auto key = m_config.m_prefix + ":"
                                 + m_adapters->get_type() + ":" + working_id
                                 + ":" + type + "s";
                foreign_key_proxy_keys.push_back(key);
            }
        }
    }
    else if (query.is_index()) {
        const auto id_request_key =
            get_field_key(query.get_index().first, query.get_index().second);
        const auto response = m_client->make_request(
            {KeyValueStoreRequestMethod::STRING_GET,
             {id_request_key},
             m_config.m_endpoint});
        error_check("STRING_GET", response.get());
        const std::string working_id = response->items()[0];
        if (response->items().empty() || working_id.empty()) {
            return;
        }
        string_get_keys.push_back(get_item_key(working_id));
        for (const auto& [type, name] : foreign_key_proxies) {
            const auto key = m_config.m_prefix + ":" + m_adapters->get_type()
                             + ":" + working_id + ":" + type + "s";
            foreign_key_proxy_keys.push_back(key);
        }
    }
    else {
        const auto response = m_client->make_request(
            {KeyValueStoreRequestMethod::SET_LIST,
             {get_set_key()},
             m_config.m_endpoint});
        error_check("SET_LIST", response.get());

        if (!response->ids().empty()) {
            for (const auto& id : response->ids()[0]) {
                string_get_keys.push_back(get_item_key(id));
                for (const auto& [type, name] : foreign_key_proxies) {
                    const auto key = m_config.m_prefix + ":"
                                     + m_adapters->get_type() + ":" + id + ":"
                                     + type + "s";
                    foreign_key_proxy_keys.push_back(key);
                }
            }
        }
    }

    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_GET, string_get_keys,
         m_config.m_endpoint});
    error_check("GET", response.get());
    if (string_get_keys.empty()) {
        return;
    }

    const auto any_empty = std::any_of(
        response->items().begin(), response->items().end(),
        [](const std::string& entry) { return entry.empty(); });
    if (any_empty) {
        return;
    }

    auto adapter = get_adapter(CrudAttributes::Format::JSON);

    Dictionary foreign_key_dict(Dictionary::Type::SEQUENCE);
    if (!foreign_key_proxy_keys.empty()) {
        const auto proxy_response = m_client->make_request(
            {KeyValueStoreRequestMethod::SET_LIST, foreign_key_proxy_keys,
             m_config.m_endpoint});
        error_check("GET", proxy_response.get());

        std::vector<std::string> proxy_value_keys;
        std::vector<std::size_t> proxy_value_offsets;
        for (std::size_t idx = 0; idx < string_get_keys.size(); idx++) {
            for (std::size_t jdx = 0; jdx < foreign_key_proxies.size(); jdx++) {
                if (proxy_response->ids().empty()) {
                    proxy_value_offsets.push_back(0);
                }
                else {
                    const auto offset = idx * foreign_key_proxies.size() + jdx;
                    const auto type   = foreign_key_proxies[jdx].first;
                    std::size_t count{0};
                    for (const auto& id : proxy_response->ids()[offset]) {
                        const auto key =
                            m_config.m_prefix + ":" + type + ":" + id;
                        proxy_value_keys.push_back(key);
                        count++;
                    }
                    proxy_value_offsets.push_back(count);
                }
            }
        }

        const auto proxy_data_response = m_client->make_request(
            {KeyValueStoreRequestMethod::STRING_GET, proxy_value_keys,
             m_config.m_endpoint});
        error_check("GET", proxy_data_response.get());

        std::size_t count = 0;
        for (std::size_t idx = 0; idx < string_get_keys.size(); idx++) {
            auto item_dict = std::make_unique<Dictionary>();
            for (std::size_t jdx = 0; jdx < foreign_key_proxies.size(); jdx++) {
                const auto offset = idx * foreign_key_proxies.size() + jdx;
                const auto name   = foreign_key_proxies[jdx].second;

                auto item_field_seq_dict =
                    std::make_unique<Dictionary>(Dictionary::Type::SEQUENCE);
                for (std::size_t kdx = 0; kdx < proxy_value_offsets[offset];
                     kdx++) {
                    const auto db_value   = proxy_data_response->items()[count];
                    auto field_entry_dict = std::make_unique<Dictionary>();
                    adapter->dict_from_string(db_value, *field_entry_dict);
                    item_field_seq_dict->add_sequence_item(
                        std::move(field_entry_dict));
                    count++;
                }

                item_dict->set_map_item(name, std::move(item_field_seq_dict));
            }
            foreign_key_dict.add_sequence_item(std::move(item_dict));
        }
    }

    Dictionary response_dict;
    adapter->from_string(response->items(), response_dict);

    if (!foreign_key_dict.is_empty()) {
        assert(!foreign_key_dict.get_sequence().empty());
        if (response_dict.get_type() == Dictionary::Type::SEQUENCE) {
            for (std::size_t idx = 0; idx < response_dict.get_sequence().size();
                 idx++) {
                response_dict.get_sequence()[idx]->merge(
                    *foreign_key_dict.get_sequence()[idx]);
            }
        }
        else {
            response_dict.merge(*foreign_key_dict.get_sequence()[0]);
        }
    }

    if (query.is_id_output_format()) {
        auto item_template = m_adapters->get_model_factory()->create();
        response_dict.get_scalars(
            item_template->get_primary_key_name(), crud_response.ids());
    }
    else if (query.is_attribute_output_format()) {
        adapter->dict_to_string(
            response_dict, crud_response.attributes().buffer());
    }
    else if (query.is_item_output_format()) {
        adapter->from_dict(response_dict, crud_response.items());
    }
}

void KeyValueCrudClient::remove(const VecCrudIdentifier& ids) const
{
    std::vector<std::string> string_remove_keys;
    std::vector<KeyValuePair> set_remove_keys;
    for (const auto& id : ids) {
        if (id.has_primary_key()) {
            string_remove_keys.push_back(get_item_key(id.get_primary_key()));
            set_remove_keys.push_back({get_set_key(), id.get_primary_key()});
        }
    }

    const auto string_response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_REMOVE, string_remove_keys,
         m_config.m_endpoint});
    error_check("STRING_REMOVE", string_response.get());

    const auto set_response = m_client->make_request(
        {KeyValueStoreRequestMethod::SET_REMOVE, set_remove_keys,
         m_config.m_endpoint});
    error_check("SET_REMOVE", string_response.get());
}

void KeyValueCrudClient::identify(
    const VecCrudIdentifier& ids, CrudResponse& response) const
{
    (void)ids;
    (void)response;
}

void KeyValueCrudClient::lock(
    const CrudIdentifier& id, CrudLockType lock_type) const
{
    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_SET,
         {KeyValuePair(get_lock_key(id.get_primary_key(), lock_type), "1")},
         m_config.m_endpoint});
    error_check("STRING_SET", response.get());
}

void KeyValueCrudClient::unlock(
    const CrudIdentifier& id, CrudLockType lock_type) const
{
    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_REMOVE,
         {get_lock_key(id.get_primary_key(), lock_type)},
         m_config.m_endpoint});
    error_check("STRING_REMOVE", response.get());
}

bool KeyValueCrudClient::is_locked(
    const CrudIdentifier& id, CrudLockType lock_type) const
{
    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_EXISTS,
         {get_lock_key(id.get_primary_key(), lock_type)},
         m_config.m_endpoint});

    error_check("STRING_EXISTS", response.get());
    return response->found()[0];
}

std::string KeyValueCrudClient::get_lock_key(
    const std::string& id, CrudLockType lock_type) const
{
    std::string lock_str = lock_type == CrudLockType::READ ? "r" : "w";
    return get_prefix() + "_lock" + lock_str + ":" + id;
}

std::string KeyValueCrudClient::get_item_key(const std::string& id) const
{
    return get_prefix() + ":" + id;
}

std::string KeyValueCrudClient::get_field_key(
    const std::string& field, const std::string& value) const
{
    return get_prefix() + "_" + field + ":" + value;
}

void KeyValueCrudClient::get_item_keys(
    const std::vector<std::string>& ids, std::vector<std::string>& keys) const
{
    for (const auto& id : ids) {
        keys.push_back(get_prefix() + ":" + id);
    }
}

std::string KeyValueCrudClient::get_prefix() const
{
    return m_config.m_prefix + ":" + m_adapters->get_type();
}

std::string KeyValueCrudClient::get_set_key() const
{
    return get_prefix() + "s";
}

void KeyValueCrudClient::error_check(
    const std::string& identifier, BaseResponse* response) const
{
    if (!response->ok()) {
        const std::string msg = "Error in kv_store " + identifier + ": "
                                + response->get_base_error().to_string();
        LOG_ERROR(msg);
        throw RequestException<CrudRequestError>({CrudErrorCode::ERROR, msg});
    }
}

}  // namespace hestia