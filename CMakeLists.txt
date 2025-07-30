cmake_minimum_required(VERSION 3.16)
project(otclient)

set(VERSION "1.0.0")

option(FRAMEWORK_SOUND "Use SOUND " ON)
option(FRAMEWORK_GRAPHICS "Use GRAPHICS " ON)
option(FRAMEWORK_XML "Use XML " ON)
option(FRAMEWORK_NET "Use NET " ON)
option(FRAMEWORK_SQL "Use SQL" OFF)
option(BUILD_STATIC_LIBRARY "Build using static libraries" OFF)
option(USE_CEF "Use CEF WebView (Windows/Linux/macOS)" OFF)

# CEF Configuration
option(USE_CEF "Enable CEF WebView support" OFF)

if(USE_CEF)
    # Try to find CEF automatically
    message(STATUS "Looking for CEF...")
    message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
    
    # Look for CEF in the local runtime directory (where setup_cef.sh copies everything)
    set(CEF_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/cef")
    message(STATUS "CEF_RUNTIME_DIR: ${CEF_RUNTIME_DIR}")
    
    if(EXISTS "${CEF_RUNTIME_DIR}/libcef.so" AND EXISTS "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.a")
        message(STATUS "CEF runtime found and ready at: ${CEF_RUNTIME_DIR}")
        
        # CEF configuration
        set(CEF_ROOT "${CEF_RUNTIME_DIR}")
        
        # Find CEF include directory (should be in the installation directory)
        set(CEF_INCLUDE_DIRS "")
        set(CEF_SEARCH_INCLUDE_PATHS
            "/opt/otclient-cef"
            "$ENV{HOME}/.local/share/otclient-cef"
            "${CMAKE_SOURCE_DIR}/cef_binary_*"
        )
        
        foreach(SEARCH_PATH ${CEF_SEARCH_INCLUDE_PATHS})
            if(SEARCH_PATH MATCHES "\\*")
                # Handle glob pattern
                file(GLOB CEF_INCLUDE_CANDIDATES "${SEARCH_PATH}")
                foreach(CANDIDATE ${CEF_INCLUDE_CANDIDATES})
                    if(EXISTS "${CANDIDATE}/include/cef_app.h")
                        set(CEF_INCLUDE_DIRS "${CANDIDATE}")
                        break()
                    endif()
                endforeach()
            else()
                if(EXISTS "${SEARCH_PATH}/include/cef_app.h")
                    set(CEF_INCLUDE_DIRS "${SEARCH_PATH}")
                    break()
                endif()
            endif()
        endforeach()
        
        if(NOT CEF_INCLUDE_DIRS)
            message(FATAL_ERROR "CEF include directory not found. Please run ./setup_cef.sh first")
        endif()
        
        message(STATUS "CEF_INCLUDE_DIRS: ${CEF_INCLUDE_DIRS}")
        set(CEF_LIBRARY_DIRS "${CEF_RUNTIME_DIR}")
        
        # CEF libraries
        set(CEF_LIBRARIES
            "${CEF_RUNTIME_DIR}/libcef.so"
            "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.a"
        )
        
        # Add CEF definitions
        add_compile_definitions(USE_CEF)
        
        # Include CEF directories
        include_directories(${CEF_INCLUDE_DIRS})
        link_directories(${CEF_LIBRARY_DIRS})
        
        message(STATUS "CEF configuration completed successfully")
    else()
        message(STATUS "CEF_RUNTIME_DIR: ${CEF_RUNTIME_DIR}")
        if(EXISTS "${CEF_RUNTIME_DIR}/libcef.so")
            message(STATUS "libcef.so exists: YES")
        else()
            message(STATUS "libcef.so exists: NO")
        endif()
        if(EXISTS "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.a")
            message(STATUS "libcef_dll_wrapper.a exists: YES")
        else()
            message(STATUS "libcef_dll_wrapper.a exists: NO")
        endif()
        message(FATAL_ERROR "CEF runtime not found. Please run ./setup_cef.sh first")
    endif()
endif()

include(src/framework/CMakeLists.txt)
include(src/client/CMakeLists.txt)

# functions map for reading backtraces
if(NOT APPLE)
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -Wl,-Map=${PROJECT_NAME}.map")
endif()

option(USE_PCH "Use precompiled header (speed up compile)" OFF)

set(executable_SOURCES
    src/main.cpp
)

if(WIN32)
    if(MSVC)
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/otcicon.o
                           COMMAND ${CMAKE_RC_COMPILER}
                           -I${CMAKE_CURRENT_SOURCE_DIR}/src
                           -fo${CMAKE_CURRENT_BINARY_DIR}/otcicon.o
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/otcicon.rc
        )
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    else()
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/otcicon.o
                           COMMAND ${CMAKE_RC_COMPILER}
                           -I${CMAKE_CURRENT_SOURCE_DIR}/src
                           -i${CMAKE_CURRENT_SOURCE_DIR}/src/otcicon.rc
                           -o ${CMAKE_CURRENT_BINARY_DIR}/otcicon.o
        )
    endif()

    set(executable_SOURCES ${executable_SOURCES} otcicon.o)
endif()

add_definitions(-D"VERSION=\\"${VERSION}\\"")

# add client executable
add_executable(${PROJECT_NAME} ${framework_SOURCES} ${client_SOURCES} ${executable_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 14)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)

# Add CEF definition directly to target
if(USE_CEF)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CEF)
    message(STATUS "Added USE_CEF definition to target")
endif()







target_link_libraries(${PROJECT_NAME} ${framework_LIBRARIES})

if(APPLE AND USE_STATIC_LIBS)
    target_link_libraries(${PROJECT_NAME} "-framework Foundation" "-framework IOKit")
endif()

if(USE_PCH)
    include(cotire)
    cotire(${PROJECT_NAME})
    message(STATUS "Use precompiled header: ON")
else()
    message(STATUS "Use precompiled header: OFF")
endif()

# installation
set(DATA_INSTALL_DIR share/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib)
install(FILES README.md BUGS LICENSE AUTHORS init.lua ${PROJECT_NAME}rc.lua DESTINATION ${DATA_INSTALL_DIR})
install(DIRECTORY data modules DESTINATION ${DATA_INSTALL_DIR} PATTERN ".git" EXCLUDE)

# add "make run"
add_custom_target(run COMMAND ${PROJECT_NAME} DEPENDS ${PROJECT_NAME} WORKING_DIRECTORY ${CMAKE_PROJECT_DIR})
