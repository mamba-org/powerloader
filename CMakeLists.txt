project(powerloader)
cmake_minimum_required(VERSION 3.20)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CLI11 REQUIRED)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(libpowerloader
    src/downloader.cpp
    src/fastest_mirror.cpp
    src/mirror.cpp
    src/target.cpp
    src/zck.cpp
    src/curl.cpp
    src/url.cpp
    src/mirrors/oci.cpp
    src/mirrors/s3.cpp
    src/uploader/s3_upload.cpp
    src/uploader/oci_upload.cpp
)

add_executable(powerloader src/cli/main.cpp)
add_executable(dpowerloader src/main.cpp)
add_executable(upowerloader src/upload.cpp)

set_property(TARGET powerloader PROPERTY CXX_STANDARD 17)

option(WITH_ZCHUNK "Enable zchunk" ON)

target_include_directories(powerloader PUBLIC
    include
    ${CURL_INCLUDE_DIRS}
)

target_include_directories(dpowerloader PUBLIC
    include
    ${CURL_INCLUDE_DIRS}
)

if (WITH_ZCHUNK)
    target_compile_definitions(powerloader PUBLIC WITH_ZCHUNK)
    target_compile_definitions(dpowerloader PUBLIC WITH_ZCHUNK)
endif()

target_link_libraries(libpowerloader ${CURL_LIBRARIES} fmt zck ${OPENSSL_LIBRARIES})
target_link_libraries(powerloader libpowerloader CLI11::CLI11)
target_link_libraries(dpowerloader libpowerloader)
target_link_libraries(upowerloader libpowerloader)

option(ENABLE_TESTS "Enable tests" ON)

# Tests
# =====
if (ENABLE_TESTS)
    add_subdirectory(test)
endif()
