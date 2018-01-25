cmake_minimum_required (VERSION 2.8)

include (CodeGenerator)

generate_code (
  MODELS ${CMAKE_CURRENT_SOURCE_DIR}/interface
  SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}/../gst-plugins
      ${KmsGstCommons_INCLUDE_DIRS}
  SERVER_IMPL_LIB_EXTRA_LIBRARIES
      ${KmsGstCommons_LIBRARIES}
  SERVER_STUB_DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/implementation/objects
)
