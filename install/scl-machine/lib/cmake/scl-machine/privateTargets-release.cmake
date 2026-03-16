#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "inference-module" for configuration "Release"
set_property(TARGET inference-module APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(inference-module PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libinference-module.so"
  IMPORTED_SONAME_RELEASE "libinference-module.so"
  )

list(APPEND _cmake_import_check_targets inference-module )
list(APPEND _cmake_import_check_files_for_inference-module "${_IMPORT_PREFIX}/lib/extensions/libinference-module.so" )

# Import target "solution-module" for configuration "Release"
set_property(TARGET solution-module APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(solution-module PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libsolution-module.so"
  IMPORTED_SONAME_RELEASE "libsolution-module.so"
  )

list(APPEND _cmake_import_check_targets solution-module )
list(APPEND _cmake_import_check_files_for_solution-module "${_IMPORT_PREFIX}/lib/extensions/libsolution-module.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
