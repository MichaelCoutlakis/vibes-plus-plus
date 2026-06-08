find_package(Boost CONFIG REQUIRED COMPONENTS multi_index)
find_package(fmt CONFIG REQUIRED)
function(vpp_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE vpp::vpp fmt::fmt)
endfunction()

vpp_example(keyed_vector)
vpp_example(ola_frame_buffer)