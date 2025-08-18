# Smart VORBIS finder that supports both vcpkg and system libraries
#  VORBIS_FOUND - system has VORBIS
#  VORBIS_INCLUDE_DIR - the VORBIS include directory
#  VORBIS_LIBRARY - the VORBIS library

# Detect if we're using vcpkg - if so, skip our custom finder entirely
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "Vorbis: vcpkg detected, skipping custom finder")
    # Exit early and let vcpkg handle it via the standard CMake integration
    return()
endif()

# Traditional system library search
message(STATUS "Vorbis: Using system library search")

FIND_PATH(VORBIS_INCLUDE_DIR NAMES vorbis/codec.h)
SET(_VORBIS_STATIC_LIBS libvorbis.a)
SET(_VORBIS_SHARED_LIBS libvorbis.dll.a vorbis)
IF(USE_STATIC_LIBS)
    FIND_LIBRARY(VORBIS_LIBRARY NAMES ${_VORBIS_STATIC_LIBS} ${_VORBIS_SHARED_LIBS})
ELSE()
    FIND_LIBRARY(VORBIS_LIBRARY NAMES ${_VORBIS_SHARED_LIBS} ${_VORBIS_STATIC_LIBS})
ENDIF()
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Vorbis DEFAULT_MSG VORBIS_LIBRARY VORBIS_INCLUDE_DIR)
MARK_AS_ADVANCED(VORBIS_LIBRARY VORBIS_INCLUDE_DIR)
