file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
  "*.cpp" "*.hpp"
)
list(FILTER SOURCES EXCLUDE REGEX "/test/")

add_library(training-module SHARED ${SOURCES})

target_link_libraries(training-module
  LINK_PUBLIC sc-machine::sc-memory
)

target_include_directories(training-module
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(training-module
  PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/extensions"
)