find_package(GTest)

include_directories(${GTEST_INCLUDE_DIRS} SYSTEM)


set(TEST_SRCS
    test_s3.cpp
    test_fileio.cpp
    test_url.cpp
)

add_executable(test_powerloader ${TEST_SRCS})

target_link_libraries(test_powerloader
    PRIVATE
        ${powerloader_dependency}
        GTest::GTest
        GTest::Main
)
set_property(TARGET test_powerloader PROPERTY CXX_STANDARD 17)

add_custom_target(test
    COMMAND test_powerloader
    DEPENDS test_powerloader
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${powerloader_dependency}>
)
