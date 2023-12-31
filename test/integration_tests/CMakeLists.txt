set(INTEGRATION_TEST_MODULE ${PROJECT_NAME}_integration_tests)

set(INTEGRATION_TEST_SOURCES
    base/common/TestSystemUtils.cc
    base/network/TestBasicHttpServer.cc
    base/network/TestCurlClient.cc
    hestia/TestHestiaWebApp.cc
    hestia/TestHestiaS3WebApp.cc
    main.cc
    )

    if(${HESTIA_WITH_PROXYGEN})
    LIST(APPEND INTEGRATION_TEST_SOURCES
    base/storage/TestProxygenServer.cc
    base/storage/TestProxygenMockPhobos.cc
    )
    endif()

    if(${HESTIA_TEST_REDIS})
    LIST(APPEND INTEGRATION_TEST_SOURCES
        base/storage/TestRedisKeyValueStore.cc
    )
    endif()

    if(${HESTIA_WITH_PHOBOS})
    LIST(APPEND INTEGRATION_TEST_SOURCES
        base/storage/TestPhobosBackend.cc
    )
    endif()

add_executable(${INTEGRATION_TEST_MODULE} ${INTEGRATION_TEST_SOURCES})

target_link_libraries(${INTEGRATION_TEST_MODULE} PRIVATE
    hestia_lib
    hestia_mocks
    hestia_test_utils
    Catch2::Catch2
    ${PLATFORM_LIBS})

if(${HESTIA_TEST_REDIS})
    target_compile_definitions(${INTEGRATION_TEST_MODULE} PUBLIC TEST_REDIS)
endif()

if(${HESTIA_WITH_PHOBOS})
    add_dependencies(${INTEGRATION_TEST_MODULE} hestia_phobos_plugin)
    target_compile_definitions(${INTEGRATION_TEST_MODULE} PUBLIC HAS_PHOBOS)
endif()

if(${HESTIA_WITH_PROXYGEN})
    add_dependencies(hestia_test_utils libzstd_static)
    target_link_libraries(${INTEGRATION_TEST_MODULE} PRIVATE hestia_proxygen_plugin)
    target_link_directories(${INTEGRATION_TEST_MODULE} PUBLIC ${CMAKE_BINARY_DIR}/lib)
    target_include_directories(${INTEGRATION_TEST_MODULE} PUBLIC ${PROJECT_BINARY_DIR})
    target_compile_definitions(${INTEGRATION_TEST_MODULE} PUBLIC HAVE_PROXYGEN)
endif()

add_dependencies(${INTEGRATION_TEST_MODULE} hestia_mock_phobos_plugin)

target_code_coverage(${INTEGRATION_TEST_MODULE} ALL)

catch_discover_tests(${INTEGRATION_TEST_MODULE})
