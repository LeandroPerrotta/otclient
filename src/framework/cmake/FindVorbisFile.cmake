# Smart VORBISFILE finder that supports both vcpkg and system libraries
#  VORBISFILE_FOUND - system has VORBISFILE
#  VORBISFILE_INCLUDE_DIR - the VORBISFILE include directory
#  VORBISFILE_LIBRARY - the VORBISFILE library

# Detect if we're using vcpkg - if so, skip our custom finder entirely
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "VorbisFile: vcpkg detected, skipping custom finder")
    # Exit early and let vcpkg handle it via the standard CMake integration
    return()
endif()

# Traditional system library search
message(STATUS "VorbisFile: Using system library search")

FIND_PATH(VORBISFILE_INCLUDE_DIR NAMES vorbis/vorbisfile.h)
SET(_VORBISFILE_STATIC_LIBS libvorbisfile.a)
SET(_VORBISFILE_SHARED_LIBS libvorbisfile.dll.a vorbisfile)
IF(USE_STATIC_LIBS)
    FIND_LIBRARY(VORBISFILE_LIBRARY NAMES ${_VORBISFILE_STATIC_LIBS} ${_VORBISFILE_SHARED_LIBS})
ELSE()
    FIND_LIBRARY(VORBISFILE_LIBRARY NAMES ${_VORBISFILE_SHARED_LIBS} ${_VORBISFILE_STATIC_LIBS})
ENDIF()
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VorbisFile DEFAULT_MSG VORBISFILE_LIBRARY VORBISFILE_INCLUDE_DIR)
MARK_AS_ADVANCED(VORBISFILE_LIBRARY VORBISFILE_INCLUDE_DIR)
