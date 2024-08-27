

add_library(oscpp_lib
  cflock.cpp
  excos.cpp
  fdentries.cpp
  os.cpp
  osutil.cpp
  oscaps.cpp
    )

target_link_libraries(oscpp_lib PUBLIC
    Qt5::Core
    ${CMAKE_DL_LIBS}
    lib_util
    cap
)


