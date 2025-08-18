# Smart OpenAL finder that supports both vcpkg and system libraries
#  OPENAL_FOUND - system has OPENAL
#  OPENAL_INCLUDE_DIR - the OPENAL include directory
#  OPENAL_LIBRARY - the OPENAL library

# Detect if we're using vcpkg - if so, skip our custom finder entirely
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "OpenAL: vcpkg detected, skipping custom finder")
    # Exit early and let vcpkg handle it via the standard CMake integration
    return()
endif()

# Traditional system library search (Linux/macOS/manual installs)
message(STATUS "OpenAL: Using system library search")

SET(OPENAL_APPLE_PATHS ~/Library/Frameworks /Library/Frameworks)
FIND_PATH(OPENAL_INCLUDE_DIR al.h PATH_SUFFIXES AL OpenAL PATHS ${OPENAL_APPLE_PATHS})
SET(_OPENAL_STATIC_LIBS libOpenAL.a libal.a libopenal.a libOpenAL32.a)
SET(_OPENAL_SHARED_LIBS libOpenAL.dll.a libal.dll.a libopenal.dll.a libOpenAL32.dll.a OpenAL al openal OpenAL32)
IF(USE_STATIC_LIBS)
    FIND_LIBRARY(OPENAL_LIBRARY NAMES ${_OPENAL_STATIC_LIBS} ${_OPENAL_SHARED_LIBS} PATHS ${OPENAL_APPLE_PATHS})
ELSE()
    FIND_LIBRARY(OPENAL_LIBRARY NAMES ${_OPENAL_SHARED_LIBS} ${_OPENAL_STATIC_LIBS} PATHS ${OPENAL_APPLE_PATHS})
ENDIF()
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenAL DEFAULT_MSG OPENAL_LIBRARY OPENAL_INCLUDE_DIR)
MARK_AS_ADVANCED(OPENAL_LIBRARY OPENAL_INCLUDE_DIR)
