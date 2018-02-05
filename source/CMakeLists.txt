add_library(let-base INTERFACE)
target_compile_features(let-base INTERFACE cxx_std_17)
target_include_directories(let-base INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
add_library(let::base ALIAS let-base)
if(NOT MSVC)
    target_compile_options(let-base INTERFACE -Wall -Wextra -Wpedantic)
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
    let/eval.hpp
    let/eval.cpp

    let/util/args.hpp
    let/util/args.cpp
    )
add_library(let::let ALIAS let)
target_link_libraries(let PUBLIC let::base PRIVATE tao::pegtl)

add_executable(let-parse let/parser/parse-main.cpp)
target_link_libraries(let-parse PRIVATE let::let)
add_executable(let-compile let/compiler/compile-main.cpp)
target_link_libraries(let-compile PRIVATE let::let)
