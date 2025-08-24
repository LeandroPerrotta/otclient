#pragma once

#ifdef USE_CEF

// CEF utility functions and helpers

namespace cef {
    // Logs a message to both file (cef.log) and console
    // Useful for early CEF initialization when framework logger may not be available
    void logMessage(const char* message);
    
    // Logs a message with a specific prefix for different processes
    void logMessage(const char* prefix, const char* message);
}

#endif // USE_CEF