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
if(USE_CEF)
    # Try to find CEF automatically
    message(STATUS "Looking for CEF...")
    message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
    
    # Look for CEF in the local runtime directory (where setup_cef.sh copies everything)
    set(CEF_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/cef")
    message(STATUS "CEF_RUNTIME_DIR: ${CEF_RUNTIME_DIR}")
    
    # Platform-specific CEF file detection
    if(WIN32)
        # Windows: look for .dll and .lib files
        if(EXISTS "${CEF_RUNTIME_DIR}/libcef.dll" AND EXISTS "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.lib")
            message(STATUS "CEF runtime found and ready at: ${CEF_RUNTIME_DIR} (Windows)")
            
            # CEF configuration
            set(CEF_ROOT "${CEF_RUNTIME_DIR}")
            
            # Find CEF include directory (should be in the permanent installation directory)
            set(CEF_INCLUDE_DIRS "")
            set(CEF_SEARCH_INCLUDE_PATHS
                "$ENV{LOCALAPPDATA}/otclient-cef"
                "C:/Program Files/otclient-cef"
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
                message(FATAL_ERROR "CEF include directory not found. Please run ./setup_cef.ps1 first")
            endif()
            
            message(STATUS "CEF_INCLUDE_DIRS: ${CEF_INCLUDE_DIRS}")
            set(CEF_LIBRARY_DIRS "${CEF_RUNTIME_DIR}")
            
            # CEF libraries (Windows)
            set(CEF_LIBRARIES
                "${CEF_RUNTIME_DIR}/libcef.lib"
                "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.lib"
            )
        else()
            message(STATUS "CEF_RUNTIME_DIR: ${CEF_RUNTIME_DIR}")
            if(EXISTS "${CEF_RUNTIME_DIR}/libcef.dll")
                message(STATUS "libcef.dll exists: YES")
            else()
                message(STATUS "libcef.dll exists: NO")
            endif()
            if(EXISTS "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.lib")
                message(STATUS "libcef_dll_wrapper.lib exists: YES")
            else()
                message(STATUS "libcef_dll_wrapper.lib exists: NO")
            endif()
            message(FATAL_ERROR "CEF runtime not found. Please run ./setup_cef.ps1 first")
        endif()
    else()
        # Linux: look for .so and .a files
        if(EXISTS "${CEF_RUNTIME_DIR}/libcef.so" AND EXISTS "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.a")
            message(STATUS "CEF runtime found and ready at: ${CEF_RUNTIME_DIR} (Linux)")
            
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
            
            # CEF libraries (Linux)
            set(CEF_LIBRARIES
                "${CEF_RUNTIME_DIR}/libcef.so"
                "${CEF_RUNTIME_DIR}/libcef_dll_wrapper.a"
            )
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
    
    # Add CEF definitions
    add_compile_definitions(USE_CEF)
    
    # Include CEF directories
    include_directories(${CEF_INCLUDE_DIRS})
    link_directories(${CEF_LIBRARY_DIRS})
    
    # Windows-specific: Configure runtime library search
    if(WIN32)
        # On Windows, we'll use runtime PATH modification in the code to find CEF DLLs
        # This keeps the DLLs organized in the /cef directory, similar to Linux rpath
        message(STATUS "CEF Windows configuration: DLLs will be found via runtime PATH modification")
    endif()
    
    message(STATUS "CEF configuration completed successfully")
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
add_executable(${PROJECT_NAME} ${framework_SOURCES} ${client_SOURCES} ${executable_SOURCES} )

if(MSVC AND NOT WINDOWS_CONSOLE)
    message(STATUS "MSVC Build console OFF")
    target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 14)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)

# Add CEF definition directly to target
if(USE_CEF)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CEF)
    message(STATUS "Added USE_CEF definition to target")
endif()

if(WIN32)
    if(MSVC)
        # Use MT_StaticRelease to match CEF configuration
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # Force static runtime for all configurations to match CEF
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
        # Prevent winsock.h conflicts
        add_compile_definitions(WIN32_LEAN_AND_MEAN)
    endif()
endif()
target_link_libraries(${PROJECT_NAME} ${framework_LIBRARIES})

# Link CEF libraries if enabled
if(USE_CEF)
    target_link_libraries(${PROJECT_NAME} ${CEF_LIBRARIES})
    message(STATUS "Linked CEF libraries: ${CEF_LIBRARIES}")
    
    # Windows: Build dedicated CEF subprocess helper for DLL isolation
    if(WIN32)
        # Create minimal CEF helper executable (WIN32 = no console window)
        add_executable(otclient_helper WIN32 src/cef_helper.cpp)
        
        # Link CEF libraries to helper with delay loading
        target_link_libraries(otclient_helper PRIVATE ${CEF_LIBRARIES} delayimp)
        target_link_options(otclient_helper PRIVATE "/DELAYLOAD:libcef.dll")
        
        # Set helper properties to match CEF runtime library settings
        set_target_properties(otclient_helper PROPERTIES
            CXX_STANDARD 14
            CXX_STANDARD_REQUIRED ON
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"  # Use dynamic runtime (MD) to match CEF
        )
        
        # Add CEF definitions to helper
        target_compile_definitions(otclient_helper PRIVATE USE_CEF)
        
        # Include CEF directories for helper
        target_include_directories(otclient_helper PRIVATE ${CEF_INCLUDE_DIRS})
        
        # Copy helper to CEF directory after build
        add_custom_command(TARGET otclient_helper POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:otclient_helper>"
            "${CEF_RUNTIME_DIR}/otclient_helper.exe"
            COMMENT "Installing CEF subprocess helper to /cef directory")
        
        # Make main target depend on helper
        add_dependencies(${PROJECT_NAME} otclient_helper)
        
        message(STATUS "CEF: Building dedicated subprocess helper")
        message(STATUS "CEF: Helper provides complete DLL isolation")
    endif()
endif()

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
