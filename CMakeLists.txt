# Copyright (c) 2021, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

project(powerloader)
cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(POWERLOADER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(POWERLOADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(POWERLOADER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# Versionning
# ===========

# Project version
file(STRINGS "${POWERLOADER_INCLUDE_DIR}/powerloader/powerloader.hpp" powerloader_version_defines
    REGEX "#define POWERLOADER_VERSION_(MAJOR|MINOR|PATCH)")
foreach(ver ${powerloader_version_defines})
    if(ver MATCHES "#define POWERLOADER_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
        set(POWERLOADER_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(POWERLOADER_VERSION
    ${POWERLAODER_VERSION_MAJOR}.${POWERLOADER_VERSION_MINOR}.${POWERLOADER_VERSION_PATCH})
message(STATUS "powerloader version: v${POWERLOADER_VERSION}")

# Binary version
# See the following URL for explanations about the binary versionning
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
file(STRINGS "${POWERLOADER_INCLUDE_DIR}/powerloader/powerloader.hpp" powerloader_version_defines
    REGEX "#define POWERLOADER_BINARY_(CURRENT|REVISION|AGE)")
foreach(ver ${xeus_version_defines})
    if(ver MATCHES "#define POWERLOADER_BINARY_(CURRENT|REVISION|AGE) +([^ ]+)$")
        set(POWERLOADER_BINARY_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(POWERLOADER_BINARY_VERSION
    ${POWERLOADER_BINARY_CURRENT}.${POWERLOADER_BINARY_REVISION}.${POWERLOADER_BINARY_AGE})
message(STATUS "xeus binary version: v${POWERLOADER_BINARY_VERSION}")

# Build options
# =============

# Compilation options
option(WITH_ZCHUNK "Enable zchunk" ON)
option(DEV "Enable dev" OFF)
option(BUILD_SHARED_LIBS "Build as shared libraries" ON)

# Test options
option(ENABLE_TESTS "Enable tests" ON)
option(ENABLE_PYTHON "Enable python bindings" ON)

# Dependencies
# ============

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CLI11 REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(tl-expected REQUIRED)
find_package(spdlog REQUIRED)

if (WITH_ZCHUNK)
    find_library(ZCK_LIBRARY zck REQUIRED)
    find_file(ZCK_H_FILE NAMES zck.h REQUIRED)
    message("Found file: ${ZCK_H_FILE}")
    get_filename_component(ZCK_INCLUDE_DIRS ${ZCK_H_FILE} DIRECTORY)
endif()

# Source files
# ============

set(LIBPOWERLOADER_PUBLIC_HEADERS
    ${POWERLOADER_INCLUDE_DIR}/powerloader/mirrors/oci.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/mirrors/s3.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/context.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/curl.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/export.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/download_target.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/downloader.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/enums.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/errors.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/fastest_mirror.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/fileio.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/mirror.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/powerloader.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/url.hpp
    ${POWERLOADER_INCLUDE_DIR}/powerloader/utils.hpp
)

set(LIBPOWERLOADER_SRCS
    ${POWERLOADER_SOURCE_DIR}/target.hpp
    ${POWERLOADER_SOURCE_DIR}/mirrors/oci.cpp
    ${POWERLOADER_SOURCE_DIR}/mirrors/s3.cpp
    ${POWERLOADER_SOURCE_DIR}/uploader/oci_upload.cpp
    ${POWERLOADER_SOURCE_DIR}/uploader/s3_upload.cpp
    ${POWERLOADER_SOURCE_DIR}/context.cpp
    ${POWERLOADER_SOURCE_DIR}/curl.cpp
    ${POWERLOADER_SOURCE_DIR}/download_target.cpp
    ${POWERLOADER_SOURCE_DIR}/downloader.cpp
    ${POWERLOADER_SOURCE_DIR}/fastest_mirror.cpp
    ${POWERLOADER_SOURCE_DIR}/mirror.cpp
    ${POWERLOADER_SOURCE_DIR}/target.cpp
    ${POWERLOADER_SOURCE_DIR}/url.cpp
    ${POWERLOADER_SOURCE_DIR}/utils.cpp
)
if (WITH_ZCHUNK)
    list(APPEND LIBPOWERLOADER_SRCS ${POWERLOADER_SOURCE_DIR}/zck.cpp
                                    ${POWERLOADER_SOURCE_DIR}/zck.hpp)
endif()

# Targets and link
# ================

add_library(libpowerloader
    ${LIBPOWERLOADER_SRCS}
    ${LIBPOWERLOADER_PUBLIC_HEADERS} # Makes headers visible in IDEs
)

set_target_properties(libpowerloader
    PROPERTIES PREFIX ""
)

get_target_property(library_type libpowerloader TYPE)
if(library_type STREQUAL SHARED_LIBRARY)
    # As a shared library: build exports symbols, usage will import.
    target_compile_definitions(libpowerloader PRIVATE POWERLOADER_EXPORTS)
elseif(library_type STREQUAL STATIC_LIBRARY)
    # As a static library: no symbol import/export from any side.
    target_compile_definitions(libpowerloader PUBLIC POWERLOADER_STATIC)
else()
    message(ERROR "libpowerloader used as neither shared or static library: not supported")
endif()

target_include_directories(
    libpowerloader
    PUBLIC
        $<BUILD_INTERFACE:${POWERLOADER_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${POWERLOADER_SOURCE_DIR}
)

if (MSVC)
    target_compile_definitions(libpowerloader PUBLIC -DNOMINMAX)
    target_compile_options(libpowerloader
        PRIVATE
            /wd4275 # Disable warning C4275: We know that we use templates instances in our symbol exports.
            /wd4251 # Disable warning C4251: We know that we use templates instances in our symbol exports.
            /permissive-
#             /W4 # Recommended warning level 4
    )
else()
    target_compile_options(libpowerloader
        PRIVATE
            -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic
    )
endif()


if (MSVC)
    set(SPDLOG_TARGET spdlog::spdlog_header_only)
    set(WINSOCK_LIBS wsock32 ws2_32)
else()
    set(SPDLOG_TARGET spdlog::spdlog)
endif()
target_link_libraries(libpowerloader
    PUBLIC
        tl::expected
        ${SPDLOG_TARGET}
    PRIVATE
        ${CURL_LIBRARIES}
        ${ZCK_LIBRARY}
        ${OPENSSL_LIBRARIES}
        ${WINSOCK_LIBS}
)


add_executable(powerloader ${POWERLOADER_SOURCE_DIR}/cli/main.cpp)
target_include_directories(powerloader PUBLIC
    ${CURL_INCLUDE_DIRS}
    ${ZCK_INCLUDE_DIRS}
)
target_link_libraries(powerloader libpowerloader CLI11::CLI11 yaml-cpp)

if (DEV)
    add_executable(dpowerloader ${POWERLOADER_SOURCE_DIR}/main.cpp)
    target_link_libraries(dpowerloader libpowerloader)
    if (WITH_ZCHUNK)
        target_compile_definitions(dpowerloader PUBLIC WITH_ZCHUNK)
    endif()
endif()

if (WITH_ZCHUNK)
    target_compile_definitions(powerloader PUBLIC WITH_ZCHUNK)
    target_compile_definitions(libpowerloader PUBLIC WITH_ZCHUNK)
    if (MSVC)
        target_compile_definitions(libpowerloader PUBLIC /wd4996)
    endif()
endif()

# Tests
# =====

if (ENABLE_TESTS)
    add_subdirectory(test)
endif()

if (ENABLE_PYTHON)
    add_subdirectory(src/python)
endif()

# Installation
# ============

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LIBMAMBA_CMAKECONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for libpowerloaderConfig.cmake")

install(TARGETS powerloader libpowerloader
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ # source directory
        DESTINATION "include" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "**.**" # select all files
)
