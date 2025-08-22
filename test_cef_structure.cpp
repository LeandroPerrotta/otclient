#include <iostream>
#include <typeinfo>

// Include CEF headers
#ifdef USE_CEF
#include "include/cef_browser.h"
#include "include/cef_render_handler.h"
#endif

int main() {
#ifdef USE_CEF
    std::cout << "Testing CefAcceleratedPaintInfo structure..." << std::endl;
    
    // Create a dummy info structure to inspect its members
    CefAcceleratedPaintInfo info;
    
    std::cout << "Size of CefAcceleratedPaintInfo: " << sizeof(info) << " bytes" << std::endl;
    
    // Try to access different possible field names
    // This will fail at compile time if the field doesn't exist
    
    #ifdef _WIN32
    std::cout << "Testing Windows-specific fields..." << std::endl;
    
    // Test possible field names for Windows handle
    /*
    auto test1 = info.shared_handle;      // Old name
    auto test2 = info.texture_handle;     // Possible new name  
    auto test3 = info.handle;             // Simple name
    auto test4 = info.d3d_handle;         // D3D specific
    auto test5 = info.windows_handle;     // Platform specific
    */
    
    #endif
    
    // Check if there's an extra/common structure
    /*
    auto extra = info.extra;
    auto common = info.common;
    */
    
    std::cout << "Structure exploration completed" << std::endl;
#else
    std::cout << "CEF not enabled" << std::endl;
#endif
    
    return 0;
}