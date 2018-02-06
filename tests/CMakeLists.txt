if(NOT TARGET catch::main)
    get_filename_component(catch_hpp "${PROJECT_SOURCE_DIR}/extern/catch/catch.hpp" ABSOLUTE)
    get_filename_component(catch_cpp "${CMAKE_CURRENT_BINARY_DIR}/catch.cpp" ABSOLUTE)
    add_custom_command(OUTPUT catch.cpp
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${catch_hpp}" "${catch_cpp}"
        COMMENT "Copying Catch header..."
        )
    add_library(catch_main STATIC "${catch_cpp}")
    target_compile_definitions(catch_main PRIVATE CATCH_CONFIG_MAIN)
    target_include_directories(catch_main PUBLIC "${PROJECT_SOURCE_DIR}/extern")
    add_library(catch::main ALIAS catch_main)
endif()

foreach(name IN ITEMS
        parser
        rt
        compiler
        program
        libs
        )
    set(tname cpp.test.${name})
    add_executable(${tname} ${name}.cpp)
    target_link_libraries(${tname} PRIVATE catch::main let::let)
    add_test(${tname} ${tname})
endforeach()