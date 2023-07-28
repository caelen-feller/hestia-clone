#include <catch2/catch_all.hpp>

#include "HttpClient.h"
#include "HttpCrudClient.h"
#include "HttpRequest.h"
#include "StringUtils.h"
#include "UuidUtils.h"

#include "MockCrudService.h"
#include "MockCrudWebApp.h"
#include "MockModel.h"
#include "RequestContext.h"
#include "TypedCrudRequest.h"

#include <iostream>
#include <map>

class MockHttpClient : public hestia::HttpClient {
  public:
    MockHttpClient()
    {
        m_service = hestia::mock::MockCrudService::create();
        m_app = std::make_unique<hestia::mock::MockCrudWebApp>(m_service.get());
    }

    hestia::HttpResponse::Ptr make_request(
        const hestia::HttpRequest& request, hestia::Stream*) override
    {
        auto intercepted_request = request;
        intercepted_request.overwrite_path(
            hestia::StringUtils::remove_prefix(request.get_path(), m_address));

        hestia::RequestContext request_context(intercepted_request);
        m_app->on_request(&request_context);
        return std::make_unique<hestia::HttpResponse>(
            *request_context.get_response());
    }

    std::string m_address{"127.0.0.1"};
    hestia::mock::MockCrudService::Ptr m_service;
    hestia::mock::MockCrudWebApp::Ptr m_app;
};

class TestHttpCrudClientFixture {
  public:
    TestHttpCrudClientFixture()
    {
        m_http_endpoint = std::make_unique<MockHttpClient>();

        hestia::CrudClientConfig config;
        config.m_endpoint =
            m_http_endpoint->m_address + m_http_endpoint->m_app->m_api_prefix;

        auto adapters = hestia::mock::MockModel::create_adapters();
        m_client      = std::make_unique<hestia::HttpCrudClient>(
            config, std::move(adapters), m_http_endpoint.get());
    }

    std::unique_ptr<MockHttpClient> m_http_endpoint;
    std::unique_ptr<hestia::HttpCrudClient> m_client;
};

TEST_CASE_METHOD(TestHttpCrudClientFixture, "Test HttpCrudClient", "[protocol]")
{
    const int id{1234};
    m_http_endpoint->m_service->m_mock_id_generator->m_id = id;
    m_http_endpoint->m_service->m_mock_time_provider->increment();

    hestia::mock::MockModel my_model;
    my_model.set_name("my_models_name");

    hestia::TypedCrudRequest request(
        hestia::CrudMethod::CREATE, my_model, {},
        hestia::CrudQuery::OutputFormat::ITEM);

    hestia::CrudResponse response(request);
    m_client->create(request, response);

    std::cout << m_http_endpoint->m_service->m_kv_store_client->dump()
              << std::endl;
    REQUIRE(response.ok());

    auto created_model = response.get_item_as<hestia::mock::MockModel>();

    REQUIRE(created_model->id() == std::to_string(id));
    REQUIRE(created_model->name() == "my_models_name");

    hestia::CrudQuery query(
        hestia::CrudIdentifier(created_model->id()),
        hestia::CrudQuery::OutputFormat::ITEM);
    hestia::CrudRequest read_request(query, {});
    hestia::CrudResponse read_response(read_request);

    m_client->read(query, read_response);
    REQUIRE(read_response.ok());

    auto read_model = read_response.get_item_as<hestia::mock::MockModel>();

    REQUIRE(read_model->id() == std::to_string(id));
    REQUIRE(read_model->name() == "my_models_name");

    const int id1{5678};

    m_http_endpoint->m_service->m_mock_id_generator->m_id = id1;
    m_http_endpoint->m_service->m_mock_time_provider->increment();

    hestia::CrudRequest create_request(
        hestia::CrudMethod::CREATE, {}, {}, {},
        hestia::CrudQuery::OutputFormat::ITEM);
    hestia::CrudResponse create_response(create_request);
    m_client->create(create_request, create_response);

    hestia::CrudQuery query2(hestia::CrudQuery::OutputFormat::ITEM);
    hestia::CrudRequest read_request2(query2, {});
    hestia::CrudResponse read_response2(read_request2);

    m_client->read(query2, read_response2);
    REQUIRE(read_response2.ok());
    REQUIRE(read_response2.items().size() == 2);
}