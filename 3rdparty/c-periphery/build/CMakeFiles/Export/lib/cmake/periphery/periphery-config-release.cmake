#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "periphery::periphery" for configuration "Release"
set_property(TARGET periphery::periphery APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(periphery::periphery PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libperiphery.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS periphery::periphery )
list(APPEND _IMPORT_CHECK_FILES_FOR_periphery::periphery "${_IMPORT_PREFIX}/lib/libperiphery.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
