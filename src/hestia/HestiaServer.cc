#include "HestiaServer.h"

#include "DistributedHsmService.h"

#include "HsmObjectStoreClient.h"
#include "HttpClient.h"
#include "KeyValueStoreClient.h"

#include "BasicHttpServer.h"

#include "HestiaS3WebApp.h"
#include "HestiaWebApp.h"

#include "ProjectConfig.h"

#include <filesystem>
#include <set>

#include "Logger.h"

namespace hestia {
HestiaServer::HestiaServer() {}

HestiaServer::~HestiaServer() {}

OpStatus HestiaServer::run()
{
    WebApp::Ptr web_app;

    if (m_config.get_server_config().get_web_app_config().get_interface()
        == WebAppConfig::Interface::S3) {
        LOG_INFO("Running S3 interface");

        HestiaS3WebAppConfig config;
        web_app = std::make_unique<HestiaS3WebApp>(
            config, m_distributed_hsm_service.get(), m_user_service.get());
    }
    else {
        LOG_INFO("Running http interface");

        const auto static_resource_path =
            m_config.get_server_config().get_static_resource_path();

        HestiaWebAppConfig app_config;
        app_config.m_cache_static_resources =
            m_config.get_server_config().should_cache_static_resources();

        if (!static_resource_path.empty()) {
            auto full_path =
                std::filesystem::current_path() / static_resource_path;
            if (std::filesystem::is_directory(full_path)) {
                app_config.m_static_resource_dir = full_path.string();
            }
        }
        web_app = std::make_unique<HestiaWebApp>(
            m_user_service.get(), m_distributed_hsm_service.get(), app_config);
    }

    Server::Config server_config;
    server_config.m_ip        = m_config.get_server_config().get_host_address();
    server_config.m_http_port = m_config.get_server_config().get_port();
    server_config.m_block_on_launch =
        m_config.get_server_config().should_block_on_launch();

    m_server = std::make_unique<BasicHttpServer>(server_config, web_app.get());

    m_server->initialize();
    m_server->start();

    if (!m_config.get_server_config().should_block_on_launch()) {
        m_server->wait_until_bound();
    }
    else {
        m_server.reset();
    }
    return {};
}

void HestiaServer::set_app_mode(const std::string&, unsigned)
{
    if (!m_config.get_server_config().has_controller_address()) {
        LOG_INFO("Running Server in Controller mode");
        m_app_mode = ApplicationMode::SERVER_CONTROLLER;
    }
    else {
        LOG_INFO("Running Server in Worker mode");
        m_app_mode = ApplicationMode::SERVER_WORKER;
    }
}
}  // namespace hestia
