
SET(lib_orderedmap_files
    ordered_hash
    ordered_map
    ordered_set
)


add_library(lib_orderedmap
    ${lib_orderedmap_files}
 )

set_target_properties(lib_orderedmap PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(lib_orderedmap
    Qt5::Core
)
