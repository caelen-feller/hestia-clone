include(AddModule)

add_subdirectory(base_types)

add_module(
    MODULE_NAME hsm
    HEADERS
        data_placement_engine/BasicDataPlacementEngine.h 
        data_placement_engine/DataPlacementEngine.h
        data_placement_engine/DataPlacementEngineFactory.h
        hsm_service/DistributedHsmService.h
        hsm_service/HsmService.h
        hsm_service/HsmServicesFactory.h
        hsm_service/model/CompositeLayer.h
        hsm_service/requests/HsmActionResponse.h 
        hsm_service/requests/HsmActionRequest.h
        hsm_service/requests/HsmActionError.h 
        key_value_store/KeyValueStoreClientFactory.h
        object_store/DistributedHsmObjectStoreClient.h
        object_store/HsmObjectStoreClientFactory.h
        object_store/HsmObjectStoreClientManager.h
        s3/S3DatasetAdapter.h
        s3/S3HsmObjectAdapter.h
    SOURCES
        data_placement_engine/BasicDataPlacementEngine.cc
        data_placement_engine/DataPlacementEngineFactory.cc
        hsm_service/DistributedHsmService.cc
        hsm_service/HsmService.cc
        hsm_service/HsmServicesFactory.cc
        hsm_service/model/CompositeLayer.cc
        hsm_service/events/HsmEventSink.cc
        hsm_service/requests/HsmActionResponse.cc
        hsm_service/requests/HsmActionRequest.cc
        hsm_service/requests/HsmActionError.cc
        key_value_store/KeyValueStoreClientFactory.cc
        object_store/DistributedHsmObjectStoreClient.cc
        object_store/HsmObjectStoreClientFactory.cc
        object_store/HsmObjectStoreClientManager.cc
        s3/S3DatasetAdapter.cc
        s3/S3HsmObjectAdapter.cc
    INTERNAL_INCLUDE_DIRS
        data_placement_engine
        hsm_service
        hsm_service/model
        hsm_service/events
        hsm_service/requests
        key_value_store
        object_store
        namespace_service
        s3
    INTERNAL_DEPENDENCIES 
        storage
        http_client
        web
        hsm_base_types
)

add_dependencies(${PROJECT_NAME}_hsm ${PROJECT_NAME}_storage_and_plugins)
