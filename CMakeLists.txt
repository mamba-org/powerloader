project(powerloader)
cmake_minimum_required(VERSION 3.20)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CLI11 REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(tl-expected REQUIRED)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

set(LIBPOWERLOADER_SRCS
    src/context.cpp
    src/download_target.cpp
    src/downloader.cpp
    src/fastest_mirror.cpp
    src/mirror.cpp
    src/target.cpp
    src/curl.cpp
    src/url.cpp
    src/utils.cpp
    src/mirrors/oci.cpp
    src/mirrors/s3.cpp
    src/uploader/s3_upload.cpp
    src/uploader/oci_upload.cpp
)

set(LIBPOWERLOADER_HEADERS
    include/powerloader/context.hpp
    include/powerloader/curl.hpp
    include/powerloader/download_target.hpp
    include/powerloader/downloader.hpp
    include/powerloader/enums.hpp
    include/powerloader/errors.hpp
    include/powerloader/fastest_mirror.hpp
    include/powerloader/fileio.hpp
    include/powerloader/mirror.hpp
    include/powerloader/url.hpp
    include/powerloader/target.hpp
    include/powerloader/utils.hpp
)



add_executable(powerloader src/cli/main.cpp)

set_property(TARGET powerloader PROPERTY CXX_STANDARD 17)

option(WITH_ZCHUNK "Enable zchunk" ON)
option(DEV "Enable dev" OFF)

if (WITH_ZCHUNK)
    list(APPEND LIBPOWERLOADER_SRCS src/zck.cpp)
    find_library(ZCK_LIBRARY zck REQUIRED)
    find_file(ZCK_H_FILE NAMES zck.h REQUIRED)
    message("Found file: ${ZCK_H_FILE}")
    get_filename_component(ZCK_INCLUDE_DIRS ${ZCK_H_FILE} DIRECTORY)
endif()

if (MSVC)
    add_definitions(-DNOMINMAX)
endif()

add_library(libpowerloader
    ${LIBPOWERLOADER_SRCS}
)
set_target_properties(libpowerloader
    PROPERTIES PREFIX ""
)

target_include_directories(powerloader PUBLIC
    include
    ${CURL_INCLUDE_DIRS}
    ${ZCK_INCLUDE_DIRS}
)

set_target_properties(powerloader PROPERTIES PUBLIC_HEADER "${LIBPOWERLOADER_HEADERS}")

if (DEV)
    add_executable(dpowerloader src/main.cpp)
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

find_package(spdlog REQUIRED)
if (MSVC)
    set(SPDLOG_TARGET spdlog::spdlog_header_only)
else()
    set(SPDLOG_TARGET spdlog::spdlog)
endif()
target_link_libraries(libpowerloader
                    INTERFACE
                        tl::expected
                    PRIVATE
                        ${CURL_LIBRARIES}
                        ${SPDLOG_TARGET}
                        ${ZCK_LIBRARY}
                        ${OPENSSL_LIBRARIES}
                    )
target_link_libraries(powerloader libpowerloader CLI11::CLI11 yaml-cpp)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LIBMAMBA_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for libmambaConfig.cmake")

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

option(ENABLE_TESTS "Enable tests" ON)

option(ENABLE_PYTHON "Enable python bindings" ON)

# Tests
# =====
if (ENABLE_TESTS)
    add_subdirectory(test)
endif()

if (ENABLE_PYTHON)
    add_subdirectory(src/python)
endif()
