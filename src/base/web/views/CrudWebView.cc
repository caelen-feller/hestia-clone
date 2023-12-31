#include "CrudWebView.h"

#include "CrudWebPages.h"
#include "JsonUtils.h"

#include <iostream>

namespace hestia {

std::string CrudWebView::get_path(const HttpRequest& request) const
{
    const auto path =
        StringUtils::split_on_first(request.get_path(), "/" + m_type_name + "s")
            .second;
    return hestia::StringUtils::remove_prefix(path, "/");
}

HttpResponse::Ptr CrudWebView::on_get(
    const HttpRequest& request,
    HttpEvent event,
    const AuthorizationContext& auth)
{
    if (event != HttpEvent::EOM) {
        return HttpResponse::create(
            HttpResponse::CompletionStatus::AWAITING_EOM);
    }

    const auto path         = get_path(request);
    auto response           = hestia::HttpResponse::create();
    const auto content_type = request.get_header().has_html_accept_type() ?
                                  "text/html" :
                                  "application/json";
    response->header().set_content_type(content_type);

    if (path.empty()) {

        const auto query_type = request.get_header().has_html_accept_type() ?
                                    CrudQuery::OutputFormat::DICT :
                                    CrudQuery::OutputFormat::ATTRIBUTES;
        CrudQuery query(query_type);
        bool has_id{false};
        if (!request.get_queries().empty()) {
            CrudIdentifier id;
            if (const auto name_val = request.get_queries().get_item("name");
                !name_val.empty()) {
                id.set_name(name_val);
                has_id = true;
            }
            if (const auto parent_name_val =
                    request.get_queries().get_item("parent_name");
                !parent_name_val.empty()) {
                id.set_parent_name(parent_name_val);
            }
            if (const auto parent_id_val =
                    request.get_queries().get_item("parent_id");
                !parent_id_val.empty()) {
                id.set_parent_primary_key(parent_id_val);
            }

            if (has_id) {
                query.set_ids({id});
            }
        }

        auto crud_response = m_service->make_request(
            CrudRequest(query, {auth.m_user_id, auth.m_user_token}),
            m_type_name);
        if (!crud_response->ok()) {
            return HttpResponse::create(
                {HttpStatus::Code::_500_INTERNAL_SERVER_ERROR,
                 crud_response->get_error().to_string()});
        }

        if (has_id && !crud_response->found()) {
            return HttpResponse::create({HttpStatus::Code::_404_NOT_FOUND});
        }

        if (request.get_header().has_html_accept_type()) {
            std::string json_body;
            JsonUtils::to_json(*crud_response->dict(), json_body, {}, 4);
            response->set_body(
                CrudWebPages::get_item_view(m_type_name, json_body));
        }
        else {
            response->set_body(crud_response->attributes().get_buffer());
        }
    }
    else {
        const auto id = path;
        CrudQuery query(
            CrudIdentifier(id), CrudQuery::OutputFormat::ATTRIBUTES);
        auto crud_response = m_service->make_request(
            CrudRequest{query, {auth.m_user_id, auth.m_user_token}},
            m_type_name);
        if (!crud_response->ok()) {
            return HttpResponse::create(
                {HttpStatus::Code::_500_INTERNAL_SERVER_ERROR,
                 crud_response->get_error().to_string()});
        }
        if (!crud_response->found()) {
            return HttpResponse::create({HttpStatus::Code::_404_NOT_FOUND});
        }
        response->set_body(crud_response->attributes().get_buffer());
    }
    return response;
}

HttpResponse::Ptr CrudWebView::on_delete(
    const HttpRequest& request,
    HttpEvent event,
    const AuthorizationContext& auth)
{
    if (event != HttpEvent::EOM) {
        return HttpResponse::create(
            HttpResponse::CompletionStatus::AWAITING_EOM);
    }

    const auto path = get_path(request);
    if (path.empty()) {
        return HttpResponse::create(HttpStatus::Code::_400_BAD_REQUEST);
    }

    auto crud_response = m_service->make_request(
        CrudRequest{
            CrudMethod::REMOVE,
            {auth.m_user_id, auth.m_user_token},
            {CrudIdentifier(path)}},
        m_type_name);
    if (!crud_response->ok()) {
        return HttpResponse::create(
            {HttpStatus::Code::_500_INTERNAL_SERVER_ERROR,
             crud_response->get_error().to_string()});
    }
    if (!crud_response->found()) {
        return HttpResponse::create({HttpStatus::Code::_404_NOT_FOUND});
    }
    return HttpResponse::create(HttpStatus::Code::_204_NO_CONTENT);
}

HttpResponse::Ptr CrudWebView::on_put(
    const HttpRequest& request,
    HttpEvent event,
    const AuthorizationContext& auth)
{
    if (event != HttpEvent::EOM) {
        return HttpResponse::create(
            HttpResponse::CompletionStatus::AWAITING_EOM);
    }

    const auto path = get_path(request);

    auto response = HttpResponse::create();

    CrudAttributes attributes;
    attributes.set_buffer(request.body());

    const std::string parent_id = request.get_queries().get_item("parent_id");

    CrudResponse::Ptr crud_response;
    if (path.empty() && parent_id.empty()) {
        crud_response = m_service->make_request(
            CrudRequest{
                CrudMethod::CREATE,
                {auth.m_user_id, auth.m_user_token},
                {},
                attributes,
                CrudQuery::OutputFormat::ATTRIBUTES},
            m_type_name);
    }
    else {
        CrudIdentifier id;
        if (!parent_id.empty()) {
            id.set_parent_primary_key(parent_id);
        }
        else {
            id.set_primary_key(path);
        }
        crud_response = m_service->make_request(
            CrudRequest{
                CrudMethod::UPDATE,
                {auth.m_user_id, auth.m_user_token},
                {id},
                attributes,
                CrudQuery::OutputFormat::ATTRIBUTES},
            m_type_name);
    }
    if (!crud_response->ok()) {
        return HttpResponse::create(
            {HttpStatus::Code::_500_INTERNAL_SERVER_ERROR,
             crud_response->get_error().to_string()});
    }

    response->set_body(crud_response->attributes().get_buffer());
    return response;
}
}  // namespace hestia