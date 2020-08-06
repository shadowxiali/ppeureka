#  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
#
#  Use, modification and distribution are subject to the
#  Boost Software License, Version 1.0. (See accompanying file
#  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

project(ppeureka)

set(HEADERS
    config.h
    error.h
	eureka_connect.h
    eureka_agent.h
    helpers.h
    http_client.h
    http_status.h
    ppeureka.h
    response.h
    types.h
)

set(SOURCES
    all_clients.h
    http_helpers.h
    s11n.h
    s11n_types.h
    eureka_connect.cpp
    eureka_agent.cpp
    helpers.cpp
)

list(APPEND SOURCES "curl/http_client.h")
list(APPEND SOURCES "curl/http_client.cpp")
list(APPEND SOURCES "curl/http_client_pool.h")
list(APPEND SOURCES "curl/http_client_pool.cpp")

foreach(SRC ${HEADERS})
    list(APPEND SOURCES "${HEADERS_DIR}/${SRC}")
endforeach()

if (BUILD_STATIC_LIB)
    add_library(${PROJECT_NAME} STATIC
        ${SOURCES}
        ${LIBB64_SOURCES}
    )
else()
    add_library(${PROJECT_NAME} SHARED
        ${SOURCES}
        ${LIBB64_SOURCES}
    )
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_auto_type cxx_decltype cxx_static_assert cxx_rvalue_references)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ext
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${JSON11_LIBRARIES}
        ${Boost_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES} ${OPENSSL_LIBRARIES} ZLIB::ZLIB)
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation" "-framework Security")
endif ()

source_group(${PROJECT_NAME} FILES ${SOURCES})
source_group(libb64 FILES ${LIBB64_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${Ppeureka_VERSION}
    SOVERSION ${Ppeureka_VERSION}
    FOLDER ${PROJECT_NAME}
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${CMAKE_PROJECT_NAME}
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

