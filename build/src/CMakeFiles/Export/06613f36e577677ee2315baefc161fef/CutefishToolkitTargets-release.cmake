#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Cutefish::ToolkitBase" for configuration "Release"
set_property(TARGET Cutefish::ToolkitBase APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Cutefish::ToolkitBase PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libcutefish-toolkit-base.so.0.1.0"
  IMPORTED_SONAME_RELEASE "libcutefish-toolkit-base.so.0"
  )

list(APPEND _cmake_import_check_targets Cutefish::ToolkitBase )
list(APPEND _cmake_import_check_files_for_Cutefish::ToolkitBase "${_IMPORT_PREFIX}/lib/libcutefish-toolkit-base.so.0.1.0" )

# Import target "Cutefish::Toolkit" for configuration "Release"
set_property(TARGET Cutefish::Toolkit APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Cutefish::Toolkit PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libcutefish-toolkit.so.0.1.0"
  IMPORTED_SONAME_RELEASE "libcutefish-toolkit.so.0"
  )

list(APPEND _cmake_import_check_targets Cutefish::Toolkit )
list(APPEND _cmake_import_check_files_for_Cutefish::Toolkit "${_IMPORT_PREFIX}/lib/libcutefish-toolkit.so.0.1.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
