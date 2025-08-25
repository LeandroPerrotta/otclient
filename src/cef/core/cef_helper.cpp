#include "cef_helper.h"

#ifdef USE_CEF

#include <cstdio>
#include <cstring>
#include <ctime>

namespace cef {

void logMessage(const char* message) {
    logMessage("CEF", message);
}

void logMessage(const char* prefix, const char* message) {
    // Open log file for appending
    FILE* log_file = fopen("cef.log", "a");
    if (log_file) {
        // Get current time
        time_t now = time(0);
        char* timestr = ctime(&now);
        // Remove newline from timestr
        if (timestr && strlen(timestr) > 0) {
            timestr[strlen(timestr) - 1] = '\0';
        }
        
        fprintf(log_file, "[%s] [%s] %s\n", 
                timestr ? timestr : "unknown", 
                prefix ? prefix : "CEF", 
                message ? message : "");
        fflush(log_file);
        fclose(log_file);
    }
    
    // Also print to console with prefix
    printf("[%s] %s\n", prefix ? prefix : "CEF", message ? message : "");
    fflush(stdout);
}

} // namespace cef

#endif // USE_CEF