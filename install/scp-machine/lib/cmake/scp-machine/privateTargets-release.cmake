#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "scp" for configuration "Release"
set_property(TARGET scp APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(scp PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libscp.so"
  IMPORTED_SONAME_RELEASE "libscp.so"
  )

list(APPEND _cmake_import_check_targets scp )
list(APPEND _cmake_import_check_files_for_scp "${_IMPORT_PREFIX}/lib/extensions/libscp.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
