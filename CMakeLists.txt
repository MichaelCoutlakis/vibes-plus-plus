cmake_minimum_required(VERSION 3.20)
project(vibes-plus-plus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies. Threads is a usage requirement of <vpp/thread.hpp>: consumers on
# platforms that need an explicit thread library (e.g. -pthread on Linux) must
# link it, so it is propagated through the INTERFACE and resolved for consumers
# by find_dependency(Threads) in the installed package config.
find_package(Threads REQUIRED)

# Header-only library target
add_library(vpp INTERFACE)
add_library(vpp::vpp ALIAS vpp)

target_include_directories(vpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(vpp INTERFACE cxx_std_17)
target_link_libraries(vpp INTERFACE Threads::Threads)

# Install
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(VPP_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/vpp)

install(TARGETS vpp EXPORT vppTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Exported targets go in their own file; the package config (below) includes it
# after resolving dependencies, so a consumer only needs find_package(vpp).
install(EXPORT vppTargets
    FILE vppTargets.cmake
    NAMESPACE vpp::
    DESTINATION ${VPP_INSTALL_CMAKEDIR}
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/vppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/vppConfig.cmake
    INSTALL_DESTINATION ${VPP_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/vppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/vppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/vppConfigVersion.cmake
    DESTINATION ${VPP_INSTALL_CMAKEDIR}
)

# Tests (opt-in via -DBUILD_TESTING=ON)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Examples (opt-in via -DBUILD_EXAMPLES=ON)
option(BUILD_EXAMPLES "Build example programs" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
