# Copyright (c) 2021, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required(VERSION 3.20)
project(powerloader)

set(CMAKE_CXX_STANDARD 17)
set(POWERLOADER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(POWERLOADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(POWERLOADER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

if (MSVC)
    message("Force Release build on Windows to prevent segfaults")
    set(CMAKE_BUILD_TYPE Release)
endif()
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
    "${POWERLOADER_VERSION_MAJOR}.${POWERLOADER_VERSION_MINOR}.${POWERLOADER_VERSION_PATCH}")
message(STATUS "powerloader version: v${POWERLOADER_VERSION}")

# Binary version
# See the following URL for explanations about the binary versionning
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
file(STRINGS "${POWERLOADER_INCLUDE_DIR}/powerloader/powerloader.hpp" powerloader_version_defines
    REGEX "#define POWERLOADER_BINARY_(CURRENT|REVISION|AGE)")
foreach(ver ${powerloader_version_defines})
    if(ver MATCHES "#define POWERLOADER_BINARY_(CURRENT|REVISION|AGE) +([^ ]+)$")
        set(POWERLOADER_BINARY_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(POWERLOADER_BINARY_VERSION
    ${POWERLOADER_BINARY_CURRENT}.${POWERLOADER_BINARY_REVISION}.${POWERLOADER_BINARY_AGE})
message(STATUS "powerloader binary version: v${POWERLOADER_BINARY_VERSION}")

# Build options
# =============

# Compilation options
option(WITH_ZCHUNK "Enable zchunk" ON)
option(DEV "Enable dev" OFF)
option(BUILD_SHARED "Build shared powerloader library" OFF)
option(BUILD_STATIC "Build static powerloader library" OFF)
option(BUILD_EXE "Build powerloader executable" OFF)
option(WITH_ZSTD "Build powerloader with zstd" ON)

# Test options
option(ENABLE_TESTS "Enable tests" ON)
option(ENABLE_PYTHON "Enable python bindings" OFF)

if (WITH_ZCHUNK)
    find_library(ZCK_LIBRARY zck REQUIRED)
    find_file(ZCK_H_FILE NAMES zck.h REQUIRED)
    get_filename_component(ZCK_INCLUDE_DIRS ${ZCK_H_FILE} DIRECTORY)
endif()

if (WITH_ZSTD)
    find_package(zstd REQUIRED)
endif()


option(ENABLE_ASAN "Enable Address-Sanitizer (currently only supported on GCC and Clang)" OFF)

if(ENABLE_ASAN)
    message(WARNING "Address-Sanitizer instrumentation will be injected into binaries - do not release these binaries")
    add_compile_options(-fno-omit-frame-pointer  -fsanitize=address)
    add_link_options(-fno-omit-frame-pointer  -fsanitize=address)
endif()

if(ENABLE_TSAN)
    message(WARNING "Thread-Sanitizer instrumentation will be injected into binaries - do not release these binaries")
    add_compile_options(-fno-omit-frame-pointer  -fsanitize=thread)
    add_link_options(-fno-omit-frame-pointer  -fsanitize=thread)
endif()

if(ENABLE_USAN)
    message(WARNING "Undefined-Sanitizer instrumentation will be injected into binaries - do not release these binaries")
    add_compile_options(-fno-omit-frame-pointer  -fsanitize=undefined)
    add_link_options(-fno-omit-frame-pointer  -fsanitize=undefined)
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
    ${POWERLOADER_SOURCE_DIR}/target.hpp
    ${POWERLOADER_SOURCE_DIR}/curl_internal.hpp
)
if (WITH_ZCHUNK)
    list(APPEND LIBPOWERLOADER_SRCS ${POWERLOADER_SOURCE_DIR}/zck.cpp
                                    ${POWERLOADER_SOURCE_DIR}/zck.hpp)
endif()

if (WITH_ZSTD)
    list(APPEND LIBPOWERLOADER_SRCS ${POWERLOADER_SOURCE_DIR}/compression.cpp)
endif()

# Targets and link
# ================

macro(libpowerloader_create_target target_name linkage deps_linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if (NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif ()

    string(TOUPPER "${deps_linkage}" deps_linkage_upper)
    if (NOT ${deps_linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library dependencies linkage: ${deps_linkage_upper}")
    endif ()

    # Output
    # ======
    add_library(${target_name} ${linkage_upper} ${LIBPOWERLOADER_SRCS} ${LIBPOWERLOADER_PUBLIC_HEADERS})
    set_target_properties(
        ${target_name}
        PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${output_name}")

    if (${linkage_upper} STREQUAL "STATIC")
        # As a static library: no symbol import/export from any side.
        target_compile_definitions(${target_name} PUBLIC POWERLOADER_STATIC)
    else ()
        # As a shared library: build exports symbols, usage will import.
        target_compile_definitions(${target_name} PRIVATE POWERLOADER_EXPORTS)
    endif()

    target_include_directories(
        ${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${POWERLOADER_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${POWERLOADER_SOURCE_DIR}
    )

    if (MSVC)
        target_compile_definitions(${target_name} PUBLIC -DNOMINMAX)
        target_compile_options(${target_name}
            PRIVATE
                /wd4275 # Disable warning C4275: We know that we use templates instances in our symbol exports.
                /wd4251 # Disable warning C4251: We know that we use templates instances in our symbol exports.
                /permissive-
                #/W4 # Recommended warning level 4
       )
    else()
        target_compile_options(${target_name}
            PRIVATE
                -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic
                -fvisibility=hidden
        )
    endif()

    # Commmon dependencies
    # ============

    find_package(tl-expected REQUIRED)
    find_package(spdlog REQUIRED)
    find_package(fmt REQUIRED)

    if (${deps_linkage_upper} STREQUAL "STATIC")
        message("   -> Statically linking against libpowerloader (static) dependencies")
        if (UNIX)
            set(REQUIRED_STATIC_DEPS
                libcurl.a
                libssh2.a

                libgssapi_krb5.a
                libkrb5.a
                libk5crypto.a
                libkrb5support.a
                libcom_err.a

                libssl.a
                libcrypto.a

                libzck.a

                libnghttp2.a

                libzstd.a
                libz.a
                )

            set(STATIC_DEPS "")
            foreach(LIB ${REQUIRED_STATIC_DEPS})
                message("Searching for ${LIB}")
                set(TMP_LIB "${LIB}-NOTFOUND")
                find_library(TMP_LIB REQUIRED NAMES "${LIB}")
                list(APPEND STATIC_DEPS "${TMP_LIB}")
            endforeach(LIB)

            message("   -> Found static dependencies:")
            foreach(LIB ${STATIC_DEPS})
                message("      - ${LIB}")
            endforeach(LIB)

            if (APPLE)
                set (POWERLOADER_FORCE_DYNAMIC_LIBS resolv)
                target_link_options(${target_name} PRIVATE -static-libstdc++)

                find_library(SECURITY_LIBRARY Security)
                find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
                find_library(COREFOUNDATION_LIBRARY CoreFoundation)
                message("Found library: ${SECURITY_LIBRARY}\n${COREFOUNDATION_LIBRARY}\n${SYSTEMCONFIGURATION_LIBRARY}")
                target_link_libraries(${target_name} PRIVATE
                    ${COREFOUNDATION_LIBRARY}
                    ${SECURITY_LIBRARY}
                    ${SYSTEMCONFIGURATION_LIBRARY}
                )
            elseif (UNIX)
                set(POWERLOADER_FORCE_DYNAMIC_LIBS rt dl resolv)
                target_link_options(${target_name} PUBLIC -static-libstdc++ -static-libgcc)
            endif()

            target_compile_definitions(${target_name} PUBLIC SPDLOG_FMT_EXTERNAL)
            target_link_libraries(${target_name}
                PUBLIC
                    tl::expected
                    spdlog::spdlog_header_only
                    fmt::fmt-header-only
                    ${STATIC_DEPS}
                    ${POWERLOADER_FORCE_DYNAMIC_LIBS}
            )
        elseif (WIN32)
            set_target_properties(${target_name} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
            target_compile_definitions(${target_name} PUBLIC _CRT_SECURE_NO_WARNINGS CURL_SATATICLIB ZCHUNK_STATIC_LIB)
            set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows-static/")
            # For Windows we have a vcpkg based build system right now.

            find_package(CURL CONFIG REQUIRED)
            find_library(CRYPTO_LIBRARIES NAMES libcrypto REQUIRED)
            find_package(zstd CONFIG REQUIRED)
            find_library(ZCHUNK_LIBRARY NAMES libzck REQUIRED)

            target_compile_definitions(${target_name} PUBLIC SPDLOG_FMT_EXTERNAL)
            target_link_libraries(
                ${target_name}
                PUBLIC
                CURL::libcurl
                ${CRYPTO_LIBRARIES}
                ${ZCHUNK_LIBRARY}
                zstd::libzstd_static
                fmt::fmt-header-only
                spdlog::spdlog_header_only
            )

            include_directories($ENV{CONDA_PREFIX}/Library/include/)
            include_directories($ENV{VCPKG_ROOT}/installed/x64-windows-static/include/)
        endif()
    else ()
        message("   -> Dynamically linking against libpowerloader (shared) dependencies")

        find_package(CURL REQUIRED)
        find_package(OpenSSL REQUIRED)

        if (MSVC)
            set(WINSOCK_LIBS wsock32 ws2_32)
        endif()

        target_link_libraries(${target_name}
            PUBLIC
                tl::expected
                fmt::fmt
                spdlog::spdlog_header_only
            PRIVATE
                ${CURL_LIBRARIES}
                ${ZCK_LIBRARY}
                ${OPENSSL_LIBRARIES}
                ${WINSOCK_LIBS}
                zstd::libzstd_shared
    )
    endif ()

    if (WITH_ZCHUNK)
        target_compile_definitions(${target_name} PUBLIC WITH_ZCHUNK)
        if (MSVC)
            target_compile_definitions(${target_name} PUBLIC /wd4996)
        endif()
    endif ()

    if (WITH_ZSTD)
        target_compile_definitions(${target_name} PUBLIC WITH_ZSTD)
    endif ()

    list(APPEND libpowerloader_targets ${target_name})
endmacro()

set(libpowerloader_targets "")

if (BUILD_SHARED)
    message(STATUS "Adding shared libpowerloader target")
    libpowerloader_create_target(libpowerloader SHARED SHARED libpowerloader)
endif ()

if (BUILD_STATIC)
    message(STATUS "Adding static libpowerloader target")

    # On Windows, a static library should use a different output name
    # to avoid the conflict with the import library of a shared one.
    if (WIN32)
        libpowerloader_create_target(libpowerloader-static STATIC STATIC libpowerloader_static)
    else ()
        libpowerloader_create_target(libpowerloader-static STATIC STATIC libpowerloader)
    endif ()
endif ()

if (NOT (BUILD_SHARED OR BUILD_STATIC))
    message(FATAL_ERROR "Select at least a build variant for libpowerloader")
endif ()

# Dependencies for executable and tests
# =====================================

if (BUILD_SHARED)
    set(powerloader_dependency libpowerloader)
elseif (BUILD_STATIC)
    set(powerloader_dependency libpowerloader-static)
endif ()

# powerloader executable
# ======================

if (BUILD_EXE)
    add_executable(powerloader ${POWERLOADER_SOURCE_DIR}/cli/main.cpp)
    if (WIN32 AND NOT BUILD_SHARED)
        set_target_properties(powerloader PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    target_include_directories(powerloader PUBLIC
        ${CURL_INCLUDE_DIRS}
        ${ZCK_INCLUDE_DIRS}
    )

    find_package(CLI11 REQUIRED)
    find_package(yaml-cpp REQUIRED)

    target_link_libraries(powerloader ${powerloader_dependency} CLI11::CLI11 yaml-cpp)
    set_target_properties(powerloader PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

    if (DEV)
        add_executable(dpowerloader ${POWERLOADER_SOURCE_DIR}/main.cpp)
        target_link_libraries(dpowerloader ${powerloader_dependency})
        if (WITH_ZCHUNK)
            target_compile_definitions(dpowerloader PUBLIC WITH_ZCHUNK)
        endif()
    endif()

    if (WITH_ZCHUNK)
        target_compile_definitions(powerloader PUBLIC WITH_ZCHUNK)
    endif()

    list(APPEND libpowerloader_targets powerloader)
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

set(POWERLOADER_CMAKECONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for libpowerloaderConfig.cmake")

install(TARGETS ${libpowerloader_targets}
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${POWERLOADER_INCLUDE_DIR}/" # source directory
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.hpp"
        PATTERN "*.h"
)

# Makes the project importable from the build directory
export(EXPORT ${PROJECT_NAME}-targets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

# Configure 'mambaConfig.cmake' for a build tree
set(POWERLOADER_CONFIG_CODE "####### Expanded from \@POWERLOADER_CONFIG_CODE\@ #######\n")
set(POWERLOADER_CONFIG_CODE "${POWERLOADER_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n")
set(POWERLOADER_CONFIG_CODE "${POWERLOADER_CONFIG_CODE}##################################################")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${PROJECT_BINARY_DIR})

# Configure 'mambaConfig.cmake' for an install tree
set(POWERLOADER_CONFIG_CODE "")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${POWERLOADER_CMAKECONFIG_INSTALL_DIR})


write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${POWERLOADER_VERSION}
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                DESTINATION ${POWERLOADER_CMAKECONFIG_INSTALL_DIR})
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${POWERLOADER_CMAKECONFIG_INSTALL_DIR})
