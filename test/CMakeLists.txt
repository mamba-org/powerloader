find_package(GTest)

include_directories(${GTEST_INCLUDE_DIRS} SYSTEM)


set(TEST_SRCS
    test_s3.cpp
)

add_executable(test_powerloader ${TEST_SRCS})

target_link_libraries(test_powerloader PRIVATE GTest::GTest GTest::Main libpowerloader)
set_property(TARGET test_powerloader PROPERTY CXX_STANDARD 17)

add_custom_target(test COMMAND test_powerloader DEPENDS test_powerloader)
