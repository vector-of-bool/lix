cmake_minimum_required(VERSION 3.7.0)
project(lix VERSION 0.0.0 DESCRIPTION "Embeddable scripting language for C++17")

include(CMakePackageConfigHelpers)

if(WIN32)
    set(LIX_INSTALL_INFIX .)
else()
    set(LIX_INSTALL_INFIX lib/${PROJECT_NAME}-${PROJECT_VERSION})
endif()

include(CTest)
enable_testing()

include(extern/extern.cmake)

add_subdirectory(source)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

get_filename_component(lix_version_config "${CMAKE_CURRENT_BINARY_DIR}/lixConfigVersion.cmake" ABSOLUTE)
write_basic_package_version_file(
    "${lix_version_config}"
    COMPATIBILITY SameMajorVersion
    )

install(
    FILES
        "${lix_version_config}"
        lixConfig.cmake
    DESTINATION "${LIX_INSTALL_INFIX}/cmake"
    )
install(EXPORT lix-targets DESTINATION "${LIX_INSTALL_INFIX}/cmake")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_PACKAGE_CONTACT "vector-of-bool <vectorofbool@gmail.com>")
include(CPack)
