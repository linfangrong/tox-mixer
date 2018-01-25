include_directories(
  ${KmsGstCommons_INCLUDE_DIRS}
  ${GSTREAMER_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set(TOX_MIXER_SOURCES
  toxmixer.c
  kmstoxmixer.c
  kmstoxmixer.h
)

add_library(toxmixer MODULE ${TOX_MIXER_SOURCES})

target_link_libraries(toxmixer
  ${KmsGstCommons_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
)

install(
  TARGETS toxmixer
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_GST_PLUGINS_DIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
