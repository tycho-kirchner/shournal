
add_library(lib_orderedmap
  ordered_hash.h
  ordered_map.h
  ordered_set.h
)

set_target_properties(lib_orderedmap PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(lib_orderedmap
    Qt5::Core
)
