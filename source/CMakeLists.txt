add_library(let-base INTERFACE)
target_compile_features(let-base INTERFACE cxx_std_17)
target_include_directories(let-base INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
add_library(let::base ALIAS let-base)
if(NOT MSVC)
    target_compile_options(let-base INTERFACE -Wall -Wextra -Wpedantic)
    target_link_libraries(let-base INTERFACE stdc++fs)
else()
    target_compile_options(let-base PUBLIC /std:c++latest)
endif()

find_program(
    AFL_AS afl-as HINTS "${PROJECT_BINARY_DIR}/afl"
    PATH_SUFFIXES afl-2.52b
    )

message(STATUS "AFL assembler: ${AFL_AS}")

if(AFL_AS)
    get_filename_component(afl_prefix "${AFL_AS}" DIRECTORY)
    target_compile_options(let-base INTERFACE
        "$<$<CONFIG:Debug>:-B${afl_prefix}/>"
        "$<$<CONFIG:RelWithDebInfo>:-B${afl_prefix}/>"
        )
endif()

get_filename_component(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/gen" ABSOLUTE)
get_filename_component(hpp_in let/libs/mod_template.hpp.in ABSOLUTE)
get_filename_component(cpp_in let/libs/mod_template.cpp.in ABSOLUTE)
get_filename_component(gen_script gen-lib.cmake ABSOLUTE)
set(gen_lib_sources)
foreach(libname IO Enum Path File String Regex Keyword)
    get_filename_component(gen_header "${gen_dir}/let/libs/${libname}.hpp" ABSOLUTE)
    get_filename_component(gen_source "${gen_dir}/let/libs/${libname}.cpp" ABSOLUTE)
    get_filename_component(in_mod let/libs/${libname}.let ABSOLUTE)
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

add_library(let STATIC
    let/parser/parser.hpp
    let/parser/parser.cpp
    let/parser/parse.hpp
    let/parser/parse.cpp
    let/parser/node.hpp
    let/parser/node.cpp

    let/list.hpp
    let/list.cpp
    let/tuple.hpp
    let/tuple.cpp
    let/exec/context.hpp
    let/exec/context.cpp
    let/exec/module.hpp
    let/exec/module.cpp
    let/exec/fn.hpp
    let/exec/fn.cpp
    let/exec/stack.hpp
    let/exec/stack.cpp
    let/exec/kernel.hpp
    let/exec/kernel.cpp
    let/exec/closure.cpp
    let/exec/closure.cpp
    let/exec/exec.hpp
    let/exec/exec.cpp

    let/code/builder.hpp
    let/code/builder.cpp
    let/code/code.hpp
    let/code/code.cpp

    let/compiler/compile.hpp
    let/compiler/compile.cpp
    let/compiler/macro.hpp
    let/compiler/macro.cpp

    let/boxed.hpp
    let/boxed.cpp
    let/value.hpp
    let/value.cpp
    let/map.hpp
    let/map.cpp
    let/raise.hpp
    let/raise.cpp
    let/eval.hpp
    let/eval.cpp
    let/symbol.hpp
    let/symbol.cpp

    let/util/args.hpp
    let/util/args.cpp

    let/libs/libs.hpp
    ${gen_lib_sources}
    )
add_library(let::let ALIAS let)
target_link_libraries(let
    PUBLIC
        let::base
    PRIVATE
        $<BUILD_INTERFACE:tao::pegtl>
        $<BUILD_INTERFACE:hamt::hamt>
    )
target_include_directories(let PUBLIC $<BUILD_INTERFACE:${gen_dir}>)
set_property(TARGET let PROPERTY EXPORT_NAME let::let)

add_executable(let-parse let/parser/parse-main.cpp)
target_link_libraries(let-parse PRIVATE let::let)
add_executable(let-compile let/compiler/compile-main.cpp)
target_link_libraries(let-compile PRIVATE let::let)
add_executable(let-eval let/eval-main.cpp)
target_link_libraries(let-eval PRIVATE let::let)

install(
    TARGETS let let-base
    EXPORT let-targets
    RUNTIME DESTINATION ${LET_INSTALL_INFIX}/bin
    ARCHIVE DESTINATION ${LET_INSTALL_INFIX}/lib
    INCLUDES DESTINATION ${LET_INSTALL_INFIX}/include
    )

install(
    DIRECTORY let ${gen_dir}/let
    DESTINATION "${LET_INSTALL_INFIX}/include"
    FILES_MATCHING
        PATTERN *.hpp
)