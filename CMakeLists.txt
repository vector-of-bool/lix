cmake_minimum_required(VERSION 3.0.0)
project(let VERSION 0.0.0)

include(CTest)
enable_testing()


include(extern/extern.cmake)

add_subdirectory(source)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
