#include "Utils.h"
#include <vector>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/utsname.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#endif

std::string unescape_string(std::string s) {
    size_t pos = 0;
    while ((pos = s.find("\\\\", pos)) != std::string::npos) { s.replace(pos, 2, "\\"); pos += 1; }
    pos = 0;
    while ((pos = s.find("\\n", pos)) != std::string::npos) { s.replace(pos, 2, "\n"); pos += 1; }
    pos = 0;
    while ((pos = s.find("\\\"", pos)) != std::string::npos) { s.replace(pos, 2, "\""); pos += 1; }
    pos = 0;
    while ((pos = s.find("\\t", pos)) != std::string::npos) { s.replace(pos, 2, "\t"); pos += 1; }
    pos = 0;
    while ((pos = s.find("\\'", pos)) != std::string::npos) { s.replace(pos, 2, "'"); pos += 1; }
    return s;
}

std::tuple<std::string, std::string> safe_print_with_escapes(const std::string& buffer) {
    if (buffer.empty()) {
        return {"", ""};
    }
    if (buffer.back() == '\\') {
        return {buffer.substr(0, buffer.length() - 1), "\\"};
    }
    // Note: Python code's logic here for multi-char escapes like '\\n' is slightly flawed.
    // A single backslash is the only real concern for stream truncation.
    // We simplify to the correct behavior.
    return {buffer, ""};
}

std::string format_output_for_display(const std::string& output) {
    std::string formatted = output;
    size_t pos = 0;
    while ((pos = formatted.find('\n', pos)) != std::string::npos) {
        formatted.replace(pos, 1, "\\n");
        pos += 2;
    }
    pos = 0;
    while ((pos = formatted.find('\t', pos)) != std::string::npos) {
        formatted.replace(pos, 1, "\\t");
        pos += 2;
    }

    const size_t max_length = 200;
    if (formatted.length() > max_length) {
        // Correctly handle multi-byte UTF-8 characters to avoid truncation in the middle of a character
        size_t end_pos = 0;
        size_t char_count = 0;
        while(end_pos < formatted.length() && char_count < max_length){
            unsigned char c = formatted[end_pos];
            if(c < 0x80) end_pos += 1;
            else if(c < 0xE0) end_pos += 2;
            else if(c < 0xF0) end_pos += 3;
            else end_pos += 4;
            char_count++;
        }
        if (end_pos < formatted.length()) {
            formatted = formatted.substr(0, end_pos) + "...";
        }
    }
    return formatted;
}

OperatingSystem detect_operating_system() {
#ifdef _WIN32
    return OperatingSystem::Windows;
#elif defined(__APPLE__)
    return OperatingSystem::MacOS;
#elif defined(__linux__)
    return OperatingSystem::Linux;
#else
    return OperatingSystem::Unknown;
#endif
}

std::string os_to_string(OperatingSystem os) {
    switch (os) {
        case OperatingSystem::Windows:
            return "Windows";
        case OperatingSystem::Linux:
            return "Linux";
        case OperatingSystem::MacOS:
            return "macOS";
        case OperatingSystem::Unknown:
        default:
            return "Unknown";
    }
}

bool is_powershell_available() {
    OperatingSystem os = detect_operating_system();

    if (os == OperatingSystem::Windows) {
        // On Windows, PowerShell is typically available
        return true;
    } else {
        // On Linux/macOS, check if pwsh (PowerShell Core) is available
        int result = std::system("pwsh -v > /dev/null 2>&1");
        return (result == 0);
    }
}

std::vector<std::string> get_supported_shells() {
    std::vector<std::string> shells;
    OperatingSystem os = detect_operating_system();

    // Python is supported on all platforms
    shells.push_back("python");

    switch (os) {
        case OperatingSystem::Windows:
            shells.push_back("batch");
            if (is_powershell_available()) {
                shells.push_back("powershell");
            }
            break;
        case OperatingSystem::Linux:
        case OperatingSystem::MacOS:
            shells.push_back("bash");
            if (is_powershell_available()) {
                shells.push_back("powershell");
            }
            break;
        case OperatingSystem::Unknown:
        default:
            // For unknown OS, try bash as a fallback
            shells.push_back("bash");
            break;
    }

    return shells;
}
