add_library(memcached_rbac STATIC
            privileges.cc
            privilege_database.cc
            ${PROJECT_SOURCE_DIR}/include/memcached/rbac/privilege_database.h
            ${PROJECT_SOURCE_DIR}/include/memcached/rbac.h
            )

target_include_directories(memcached_rbac SYSTEM PRIVATE ${FOLLY_INCLUDE_DIR})
target_link_libraries(memcached_rbac cbsasl platform ${FOLLY_LIBRARIES})

if (COUCHBASE_KV_BUILD_UNIT_TESTS)
    add_executable(memcached_rbac_test privilege_test.cc)
    target_link_libraries(memcached_rbac_test memcached_rbac
                          gtest gtest_main)
    add_test(NAME memcached-rbac-test
             WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
             COMMAND memcached_rbac_test)
    add_sanitizers(memcached_rbac_test)
endif ()
