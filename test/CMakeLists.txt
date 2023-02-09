find_package(doctest)

include_directories(${GTEST_INCLUDE_DIRS} SYSTEM)

set(TEST_SRCS
    test_main.cpp
    test_s3.cpp
    test_fileio.cpp
    test_url.cpp
    test_compression.cpp
    test_utility.cpp
)

add_executable(test_powerloader ${TEST_SRCS})
if (WIN32 AND BUILD_STATIC)
    set_target_properties(test_powerloader PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(
    test_powerloader
    PRIVATE
    ${powerloader_dependency}
    doctest::doctest
)
set_property(TARGET test_powerloader PROPERTY CXX_STANDARD 17)

add_custom_target(test
    COMMAND test_powerloader
    DEPENDS test_powerloader
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${powerloader_dependency}>
)
