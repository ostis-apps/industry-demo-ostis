#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ps-common-lib::common-utils" for configuration "Release"
set_property(TARGET ps-common-lib::common-utils APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ps-common-lib::common-utils PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libcommon-utils.so"
  IMPORTED_SONAME_RELEASE "libcommon-utils.so"
  )

list(APPEND _cmake_import_check_targets ps-common-lib::common-utils )
list(APPEND _cmake_import_check_files_for_ps-common-lib::common-utils "${_IMPORT_PREFIX}/lib/libcommon-utils.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
