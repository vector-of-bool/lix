add_library(let-parser STATIC
    let/parser/parser.hpp
    let/parser/parser.cpp
    let/parser/parse.hpp
    let/parser/parse.cpp
    let/parser/token.cpp
    )
add_library(let::parser ALIAS let-parser)

if(MSVC)
    target_compile_options(let-parser PUBLIC /std:c++latest)
endif()

target_include_directories(let-parser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")