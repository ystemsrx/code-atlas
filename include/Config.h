#ifndef CONFIG_H
#define CONFIG_H

#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief 加载配置文件 (config.json 或 config_template.json)。
 * * 首先尝试加载 config.json。如果找不到，则尝试加载 config_template.json。
 * 如果两者都找不到或解析失败，将抛出异常。
 * * @return nlohmann::json 包含配置数据的JSON对象。
 * @throw std::runtime_error 如果配置文件找不到或无法解析。
 */
nlohmann::json load_config();

#endif // CONFIG_H
