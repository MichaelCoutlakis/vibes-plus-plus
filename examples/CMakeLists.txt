find_package(fmt CONFIG REQUIRED)
function(vpp_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE vpp::vpp fmt::fmt)
endfunction()

vpp_example(keyed_vector)
vpp_example(ola_frame_buffer)
vpp_example(functional)
vpp_example(thread)

# The mat_view example converts from Armadillo matrices, so it needs Armadillo.
find_package(Armadillo QUIET)
if(ARMADILLO_FOUND)
    vpp_example(mat_view)
    target_include_directories(example_mat_view PRIVATE ${ARMADILLO_INCLUDE_DIRS})
    target_link_libraries(example_mat_view PRIVATE ${ARMADILLO_LIBRARIES})
endif()
