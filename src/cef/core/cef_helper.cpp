#include "cef_helper.h"

#ifdef USE_CEF

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <climits>
#endif

namespace cef {

void logMessage(const char* message) {
    logMessage("CEF", message);
}

void logMessage(const char* prefix, const char* message) {
    // Create log file path in the same directory as the executable
    static std::string log_path;
    if (log_path.empty()) {
#ifdef _WIN32
        char exe_path[MAX_PATH];
        if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
            std::string exe_dir = exe_path;
            size_t pos = exe_dir.find_last_of("\\/");
            if (pos != std::string::npos) {
                exe_dir = exe_dir.substr(0, pos);
            }
            log_path = exe_dir + "\\cef.log";
        } else {
            log_path = "cef.log";
        }
#else
        char exe_path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
            std::string exe_dir = exe_path;
            size_t pos = exe_dir.find_last_of('/');
            if (pos != std::string::npos) {
                exe_dir = exe_dir.substr(0, pos);
            }
            log_path = exe_dir + "/cef.log";
        } else {
            log_path = "cef.log";
        }
#endif
    }
    
    // Open log file for appending
    FILE* log_file = fopen(log_path.c_str(), "a");
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