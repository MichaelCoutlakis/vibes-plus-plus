include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
# Keep gtest install footprint out of the parent project's install
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_ordered_registry test_ordered_registry.cpp)
target_link_libraries(test_ordered_registry PRIVATE vpp::vpp GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_ordered_registry)
