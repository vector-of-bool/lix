add_library(lix-base INTERFACE)
target_compile_features(lix-base INTERFACE cxx_std_17)
target_include_directories(lix-base INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
add_library(lix::base ALIAS lix-base)
if(NOT MSVC)
    target_compile_options(lix-base INTERFACE -Wall -Wextra -Wpedantic)
    target_link_libraries(lix-base INTERFACE stdc++fs)
else()
    target_compile_options(lix-base PUBLIC /std:c++latest)
endif()

get_filename_component(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/gen" ABSOLUTE)
get_filename_component(hpp_in lix/libs/mod_template.hpp.in ABSOLUTE)
get_filename_component(cpp_in lix/libs/mod_template.cpp.in ABSOLUTE)
get_filename_component(gen_script gen-lib.cmake ABSOLUTE)
set(gen_lib_sources)
foreach(libname IO Enum Path File String Regex Keyword Map)
    get_filename_component(gen_header "${gen_dir}/lix/libs/${libname}.hpp" ABSOLUTE)
    get_filename_component(gen_source "${gen_dir}/lix/libs/${libname}.cpp" ABSOLUTE)
    get_filename_component(in_mod lix/libs/${libname}.lix ABSOLUTE)
    set(more_deps)
    if(EXISTS "${in_mod}.extra.cpp")
        set(more_deps "${in_mod}.extra.cpp")
    endif()
    add_custom_command(
        OUTPUT "${gen_header}" "${gen_source}"
        DEPENDS "${hpp_in}" "${cpp_in}" "${in_mod}" "${gen_script}" ${more_deps}
        COMMAND "${CMAKE_COMMAND}"
            -D in_mod=${in_mod}
            -D in_hpp=${hpp_in}
            -D in_cpp=${cpp_in}
            -D out_hpp=${gen_header}
            -D out_cpp=${gen_source}
            -D name=${libname}
            -P "${gen_script}"
        COMMENT "Generating code for ${libname} library"
        )
    list(APPEND gen_lib_sources "${gen_header}" "${gen_source}")
endforeach()

add_library(lix STATIC
    lix/parser/parser.hpp
    lix/parser/parser.cpp
    lix/parser/parse.hpp
    lix/parser/parse.cpp
    lix/parser/node.hpp
    lix/parser/node.cpp

    lix/list.hpp
    lix/list.cpp
    lix/tuple.hpp
    lix/tuple.cpp
    lix/exec/context.hpp
    lix/exec/context.cpp
    lix/exec/module.hpp
    lix/exec/module.cpp
    lix/exec/fn.hpp
    lix/exec/fn.cpp
    lix/exec/stack.hpp
    lix/exec/stack.cpp
    lix/exec/kernel.hpp
    lix/exec/kernel.cpp
    lix/exec/closure.cpp
    lix/exec/closure.cpp
    lix/exec/exec.hpp
    lix/exec/exec.cpp

    lix/code/builder.hpp
    lix/code/builder.cpp
    lix/code/code.hpp
    lix/code/code.cpp
    lix/code/instr.hpp
    lix/code/instr.cpp

    lix/compiler/compile.hpp
    lix/compiler/compile.cpp
    lix/compiler/macro.hpp
    lix/compiler/macro.cpp

    lix/boxed.hpp
    lix/boxed.cpp
    lix/value.hpp
    lix/value.cpp
    lix/map.hpp
    lix/map.cpp
    lix/raise.hpp
    lix/raise.cpp
    lix/eval.hpp
    lix/eval.cpp
    lix/symbol.hpp
    lix/symbol.cpp

    lix/util/args.hpp
    lix/util/args.cpp

    lix/libs/libs.hpp
    ${gen_lib_sources}
    )
add_library(lix::lix ALIAS lix)
target_link_libraries(lix
    PUBLIC
        lix::base
    PRIVATE
        $<BUILD_INTERFACE:tao::pegtl>
        $<BUILD_INTERFACE:hamt::hamt>
    )
target_include_directories(lix PUBLIC $<BUILD_INTERFACE:${gen_dir}>)
set_property(TARGET lix PROPERTY EXPORT_NAME lix::lix)

add_executable(lix-parse lix/parser/parse-main.cpp)
target_link_libraries(lix-parse PRIVATE lix::lix)
add_executable(lix-compile lix/compiler/compile-main.cpp)
target_link_libraries(lix-compile PRIVATE lix::lix)
add_executable(lix-eval lix/eval-main.cpp)
target_link_libraries(lix-eval PRIVATE lix::lix)

install(
    TARGETS lix lix-base
    EXPORT lix-targets
    RUNTIME DESTINATION ${LIX_INSTALL_INFIX}/bin
    ARCHIVE DESTINATION ${LIX_INSTALL_INFIX}/lib
    INCLUDES DESTINATION ${LIX_INSTALL_INFIX}/include
    )

install(
    DIRECTORY lix ${gen_dir}/lix
    DESTINATION "${LIX_INSTALL_INFIX}/include"
    FILES_MATCHING
        PATTERN *.hpp
)