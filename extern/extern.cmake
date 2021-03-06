add_library(tao::pegtl INTERFACE IMPORTED)

set_target_properties(tao::pegtl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/PEGTL/include"
    INTERFACE_COMPILE_FEATURES cxx_std_14
    )

add_library(hamt::hamt INTERFACE IMPORTED)
set_target_properties(hamt::hamt PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/hamt"
    INTERFACE_COMPILE_FEATURES cxx_std_14
    )