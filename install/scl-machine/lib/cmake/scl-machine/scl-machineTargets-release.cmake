#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "scl-machine::inference" for configuration "Release"
set_property(TARGET scl-machine::inference APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(scl-machine::inference PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libinference.so"
  IMPORTED_SONAME_RELEASE "libinference.so"
  )

list(APPEND _cmake_import_check_targets scl-machine::inference )
list(APPEND _cmake_import_check_files_for_scl-machine::inference "${_IMPORT_PREFIX}/lib/libinference.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
