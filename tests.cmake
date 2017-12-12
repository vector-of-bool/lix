if(NOT TARGET catch::main)
    get_filename_component(catch_hpp "${CMAKE_CURRENT_LIST_DIR}/extern/catch/catch.hpp" ABSOLUTE)
    get_filename_component(catch_cpp "${CMAKE_CURRENT_BINARY_DIR}/catch.cpp" ABSOLUTE)
    add_custom_command(OUTPUT catch.cpp
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${catch_hpp}" "${catch_cpp}"
        COMMENT "Copying Catch header..."
        )
    add_library(catch_main STATIC "${catch_cpp}")
    target_compile_definitions(catch_main PRIVATE CATCH_CONFIG_MAIN)
    target_include_directories(catch_main PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/extern")
    add_library(catch::main ALIAS catch_main)
endif()

foreach(fname varint message)
    set(test proto11.encoder.${fname})
    add_executable(${test} proto11/encoder/${fname}.t.cpp)
    target_link_libraries(${test} PRIVATE proto11::support catch::main)
    add_test(${test} ${test})
endforeach()
