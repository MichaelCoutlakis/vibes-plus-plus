include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
# Keep gtest install footprint out of the parent project's install
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_keyed_vector test_keyed_vector.cpp)
target_link_libraries(test_keyed_vector PRIVATE vpp::vpp GTest::gmock_main)

add_executable(test_ola_frame_buffer test_ola_frame_buffer.cpp)
target_link_libraries(test_ola_frame_buffer PRIVATE vpp::vpp GTest::gmock_main)

add_executable(test_functional test_functional.cpp)
target_link_libraries(test_functional PRIVATE vpp::vpp GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(test_keyed_vector)
gtest_discover_tests(test_ola_frame_buffer)
gtest_discover_tests(test_functional)
