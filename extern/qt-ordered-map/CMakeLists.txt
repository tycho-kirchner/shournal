

add_library(lib_orderedmap
    orderedmap
 )

add_library(lib_orderedmap_shared
    orderedmap
    )

set_property(TARGET lib_orderedmap_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
hide_static_lib_symbols(lib_orderedmap_shared)

set_target_properties(lib_orderedmap PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(lib_orderedmap_shared PROPERTIES LINKER_LANGUAGE CXX)


target_link_libraries(lib_orderedmap
    Qt5::Core
)
target_link_libraries(lib_orderedmap_shared
    Qt5::Core
    )
