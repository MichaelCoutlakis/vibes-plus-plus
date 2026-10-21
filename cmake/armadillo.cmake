# Optional Armadillo, for the mat_view adapter tests and example. Packagers
# name its target differently (Conan: Armadillo::Armadillo; vcpkg, msys2 and
# Armadillo's own install: armadillo), so normalise to Armadillo::Armadillo.
# Callers test if(TARGET Armadillo::Armadillo).

find_package(Armadillo CONFIG QUIET)
if(NOT Armadillo_FOUND)
    find_package(Armadillo MODULE QUIET) # CMake's FindArmadillo: variables only
endif()

if(TARGET Armadillo::Armadillo)
    # Already namespaced.
elseif(TARGET armadillo)
    add_library(Armadillo::Armadillo ALIAS armadillo)
elseif(ARMADILLO_FOUND)
    add_library(Armadillo::Armadillo INTERFACE IMPORTED)
    target_include_directories(Armadillo::Armadillo INTERFACE ${ARMADILLO_INCLUDE_DIRS})
    target_link_libraries(Armadillo::Armadillo INTERFACE ${ARMADILLO_LIBRARIES})
endif()

if(TARGET Armadillo::Armadillo)
    message(STATUS "Armadillo found: building mat_view Armadillo tests/example")
else()
    message(STATUS "Armadillo not found: skipping mat_view Armadillo tests/example (set Armadillo_DIR)")
endif()
