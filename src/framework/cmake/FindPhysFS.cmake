# Smart PHYSFS finder that supports both vcpkg and system libraries
#  PHYSFS_FOUND - system has PHYSFS
#  PHYSFS_INCLUDE_DIR - the PHYSFS include directory
#  PHYSFS_LIBRARY - the PHYSFS library

# Detect if we're using vcpkg - if so, skip our custom finder entirely
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "PhysFS: vcpkg detected, skipping custom finder")
    # Exit early and let vcpkg handle it via the standard CMake integration
    return()
endif()

# Traditional system library search
message(STATUS "PhysFS: Using system library search")

FIND_PATH(PHYSFS_INCLUDE_DIR physfs.h PATH_SUFFIXES physfs)
SET(_PHYSFS_STATIC_LIBS libphysfs.a)
SET(_PHYSFS_SHARED_LIBS libphysfs.dll.a physfs libphysfs.so)
IF(USE_STATIC_LIBS AND BUILD_STATIC_LIBRARY)
    FIND_LIBRARY(PHYSFS_LIBRARY NAMES ${_PHYSFS_STATIC_LIBS} ${_PHYSFS_SHARED_LIBS})
ELSE()
    FIND_LIBRARY(PHYSFS_LIBRARY NAMES ${_PHYSFS_SHARED_LIBS} ${_PHYSFS_STATIC_LIBS})
ENDIF()
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PhysFS DEFAULT_MSG PHYSFS_LIBRARY PHYSFS_INCLUDE_DIR)
MARK_AS_ADVANCED(PHYSFS_LIBRARY PHYSFS_INCLUDE_DIR)
