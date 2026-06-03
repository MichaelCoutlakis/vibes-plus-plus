cmake_minimum_required(VERSION 3.20)
project(vibes-plus-plus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library target
add_library(vpp INTERFACE)
add_library(vpp::vpp ALIAS vpp)

target_include_directories(vpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(vpp INTERFACE cxx_std_17)

# Install
include(GNUInstallDirs)

install(TARGETS vpp EXPORT vppTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT vppTargets
    FILE vppConfig.cmake
    NAMESPACE vpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vpp
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
