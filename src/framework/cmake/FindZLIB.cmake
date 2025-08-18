# Smart ZLIB finder that supports both vcpkg and system libraries
#  ZLIB_FOUND - system has ZLIB
#  ZLIB_INCLUDE_DIR - the ZLIB include directory
#  ZLIB_LIBRARY - the ZLIB library

# Detect if we're using vcpkg - if so, skip our custom finder entirely
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "ZLIB: vcpkg detected, skipping custom finder")
    # Exit early and let vcpkg handle it via the standard CMake integration
    return()
endif()

# Traditional system library search
message(STATUS "ZLIB: Using system library search")

FIND_PATH(ZLIB_INCLUDE_DIR NAMES zlib.h)
SET(_ZLIB_STATIC_LIBS libz.a libzlib.a zlib1.a)
SET(_ZLIB_SHARED_LIBS libz.dll.a zdll zlib zlib1 z)
IF(USE_STATIC_LIBS)
    FIND_LIBRARY(ZLIB_LIBRARY NAMES ${_ZLIB_STATIC_LIBS} ${_ZLIB_SHARED_LIBS})
ELSE()
    FIND_LIBRARY(ZLIB_LIBRARY NAMES ${_ZLIB_SHARED_LIBS} ${_ZLIB_STATIC_LIBS})
ENDIF()
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZLIB DEFAULT_MSG ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
MARK_AS_ADVANCED(ZLIB_LIBRARY ZLIB_INCLUDE_DIR)