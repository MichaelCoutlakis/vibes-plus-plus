find_package(Boost CONFIG REQUIRED COMPONENTS multi_index)
function(vpp_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE vpp::vpp)
endfunction()

vpp_example(keyed_vector)
