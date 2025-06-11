#include "Config.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

// Helper function to get the directory where the executable is located
// This allows the program to find the config file regardless of where it's run from
std::filesystem::path get_executable_dir() {
#ifdef _WIN32
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#else
    // Implementation for Linux/macOS
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::filesystem::path(std::string(result, (count > 0) ? count : 0)).parent_path();
#endif
}

nlohmann::json load_config() {
    namespace fs = std::filesystem;
    fs::path base_dir = get_executable_dir();
    fs::path config_path = base_dir / "config.json";
    fs::path template_path = base_dir / "config_template.json";

    auto try_load = [](const fs::path& path) -> nlohmann::json {
        if (!fs::exists(path)) {
            return nullptr;
        }
        std::ifstream f(path);
        if (!f.is_open()) {
            return nullptr;
        }
        try {
            return nlohmann::json::parse(f);
        } catch (nlohmann::json::parse_error& e) {
            throw std::runtime_error("Config file parsing failed: " + path.string() + "\nError details: " + e.what());
        }
    };

    nlohmann::json config = try_load(config_path);
    if (!config.is_null()) {
        return config;
    }

    std::cout << "config.json not found, trying to use config_template.json" << std::endl;
    config = try_load(template_path);
    if (!config.is_null()) {
        std::cout << "Using config_template.json as configuration file" << std::endl;
        return config;
    }

    throw std::runtime_error("Configuration file not found!\nPlease ensure that config.json or config_template.json exists in the program directory.");
}
