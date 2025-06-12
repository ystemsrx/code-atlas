#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <tuple>
#include <vector>

/**
 * @brief Operating system types
 */
enum class OperatingSystem {
    Windows,
    Linux,
    MacOS,
    Unknown
};

/**
 * @brief 处理JSON流中的转义字符。
 * @param s 输入字符串。
 * @return 返回已反转义的字符串。
 */
std::string unescape_string(std::string s);

/**
 * @brief Detect the current operating system at runtime
 * @return The detected operating system
 */
OperatingSystem detect_operating_system();

/**
 * @brief Get a string representation of the operating system
 * @param os The operating system enum value
 * @return String name of the operating system
 */
std::string os_to_string(OperatingSystem os);

/**
 * @brief Check if PowerShell is available on the current system
 * @return true if PowerShell is available, false otherwise
 */
bool is_powershell_available();

/**
 * @brief Get the list of supported shell types for the current OS
 * @return Vector of supported shell names
 */
std::vector<std::string> get_supported_shells();

/**
 * @brief 安全地处理包含转义字符的缓冲区，防止截断不完整的转义序列。
 * @param buffer 输入缓冲区。
 * @return 一个元组: (可以安全处理的部分, 需要保留在缓冲区等待后续数据的部分)。
 */
std::tuple<std::string, std::string> safe_print_with_escapes(const std::string& buffer);

/**
 * @brief 将工具输出格式化为单行，以便在终端中简洁显示。
 * @param output 原始多行输出。
 * @return 格式化后的单行字符串。
 */
std::string format_output_for_display(const std::string& output);

#endif // UTILS_H
