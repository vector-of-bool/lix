cmake_minimum_required(VERSION 3.7.0)
project(let VERSION 0.0.0 DESCRIPTION "Embeddable scripting language for C++17")

include(CMakePackageConfigHelpers)

if(WIN32)
    set(LET_INSTALL_INFIX .)
else()
    set(LET_INSTALL_INFIX lib/${PROJECT_NAME}-${PROJECT_VERSION})
endif()

include(CTest)
enable_testing()

include(extern/extern.cmake)

add_subdirectory(source)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

get_filename_component(let_version_config "${CMAKE_CURRENT_BINARY_DIR}/letConfigVersion.cmake" ABSOLUTE)
write_basic_package_version_file(
    "${let_version_config}"
    COMPATIBILITY SameMajorVersion
    )

install(
    FILES
        "${let_version_config}"
        letConfig.cmake
    DESTINATION "${LET_INSTALL_INFIX}/cmake"
    )
install(EXPORT let-targets DESTINATION "${LET_INSTALL_INFIX}/cmake")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_PACKAGE_CONTACT "vector-of-bool <vectorofbool@gmail.com>")
include(CPack)
