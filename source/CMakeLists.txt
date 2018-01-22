add_library(let-base INTERFACE)
target_compile_features(let-base INTERFACE cxx_std_17)
target_include_directories(let-base INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
add_library(let::base ALIAS let-base)
if(NOT MSVC)
    target_compile_options(let-base INTERFACE -Wall -Wextra -Wpedantic)
else()
    target_compile_options(let-base PUBLIC /std:c++latest)
endif()
add_library(let-parser STATIC
    let/parser/parser.hpp
    let/parser/parser.cpp
    let/parser/parse.hpp
    let/parser/parse.cpp
    let/parser/node.hpp
    let/parser/node.cpp
    )
add_library(let::parser ALIAS let-parser)
target_link_libraries(let-parser PRIVATE tao::pegtl)

add_executable(let-parse let/parser/parse-main.cpp)
target_link_libraries(let-parse PRIVATE let::parser)

target_include_directories(let-parser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")