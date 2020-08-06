#  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
#
#  Use, modification and distribution are subject to the
#  Boost Software License, Version 1.0. (See accompanying file
#  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.1)

project(Ppeureka VERSION 0.0.1)

if (WIN32)
    option(BUILD_STATIC_LIB "Build Ppeureka as static library" ON)
else()
    option(BUILD_STATIC_LIB "Build Ppeureka as static library" OFF)
endif()

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)


if (${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC")

    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        message(FATAL_ERROR "GCC ${GCC_VERSION} found. Note that using of GCC version less then 4.9 not supported")
    endif()
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -ftemplate-depth=256")
elseif (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")

    # Turn off MS specific warnings that shown for standard compatible code (mostly for Boost)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)

    # Build for Windows Vista / Windows Server 2008
    add_definitions(-D_WIN32_WINNT=0x0600)
endif()

enable_testing()

find_package(json11 REQUIRED)

# Add user specified path to CURL headers/libraries into CMAKE_INCLUDE_PATH/CMAKE_LIBRARY_PATH variables.
# Otherwise CURL could not be found on Windows
if ("${CURL_ROOT}" STREQUAL "")
    set (CURL_ROOT "$ENV{CURL_ROOT}")
endif ()

if (NOT ${CURL_ROOT} STREQUAL "")
    set (CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${CURL_ROOT}/include")
    set (CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${CURL_ROOT}/lib")
endif ()
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)


set(LIBB64_DIR "${PROJECT_SOURCE_DIR}/ext/b64")
set(LIBB64_SOURCES "${LIBB64_DIR}/cdecode.h" "${LIBB64_DIR}/cdecode.c" "${LIBB64_DIR}/cencode.h" "${LIBB64_DIR}/cencode.c")

set(HEADERS_DIR "${PROJECT_SOURCE_DIR}/include/ppeureka")

if (WIN32 AND NOT BUILD_STATIC_LIB)
    message(FATAL_ERROR "Building Ppeureka as dynamic library on Windows is not supported")
endif()

add_subdirectory(src)

install(
    DIRECTORY "${HEADERS_DIR}"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(EXPORT ${CMAKE_PROJECT_NAME} DESTINATION cmake FILE PpeurekaConfig.cmake)
export(EXPORT ${CMAKE_PROJECT_NAME} FILE PpeurekaConfig.cmake)

# Generate and install pkg-config file
if (NOT WIN32 OR CYGWIN)
    if (BUILD_STATIC_LIB)
        set(Ppeureka_libs "-lPpeureka -ljson11")
    else()
        set(Ppeureka_libs "-lPpeureka")
    endif()

    configure_file(ppeureka.pc.in ppeureka.pc @ONLY)

    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/ppeureka.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )
endif()

