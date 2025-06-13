#pragma once

#include <string>

// To avoid putting it in global namespace
namespace Color {
    // Basic colors
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";

    // Bright/light colors
    const std::string LIGHT_PINK = "\033[95m";
} 