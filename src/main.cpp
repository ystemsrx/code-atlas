#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <nlohmann/json.hpp>
#include "Config.h"
#include "ApiClient.h"
#include "CodeExecutor.h"
#include "Utils.h"

#ifdef _WIN32
#include <windows.h>
#endif

// 全局指针，用于信号处理器
PythonExecutor* g_python_executor = nullptr;

void signal_handler(int signum) {
    std::cout << "\n[INFO] Exiting gracefully." << std::endl;
    // 在退出前确保Python解释器被正确关闭
    if (g_python_executor) {
        delete g_python_executor;
        g_python_executor = nullptr;
    }
    exit(signum);
}

void main_loop() {
    // 加载配置
    auto config = load_config();

    // 初始化 API 客户端和 Python 执行器
    ApiClient api_client(config);
    PythonExecutor python_executor;
    g_python_executor = &python_executor; // 设置全局指针

    // 准备消息历史
    nlohmann::json messages = nlohmann::json::array();
    if (config.contains("system") && config["system"].contains("prompt") && !config["system"]["prompt"].get<std::string>().empty()) {
        messages.push_back({
            {"role", "system"},
            {"content", config["system"]["prompt"]}
        });
    }

    // 主循环
    while (true) {
        std::cout << "> ";
        std::string user_input;
        std::getline(std::cin, user_input);
        if (std::cin.eof()) { // 处理 Ctrl+D 或文件结尾
             signal_handler(0);
        }
        std::cout << std::endl;

        if (user_input.empty()) {
            continue;
        }

        messages.push_back({{"role", "user"}, {"content", user_input}});

        // 工具调用循环
        while (true) {
            ApiResponse response = api_client.send_message(messages);

            if (response.type == ApiResponse::Type::API_ERROR) {
                std::cerr << response.error_message << std::endl;
                break; 
            }

            if (response.type == ApiResponse::Type::TOOL_CALL) {
                nlohmann::json assistant_message = {{"role", "assistant"}, {"content", response.content}};
                if (!response.tool_calls.empty()) {
                    assistant_message["tool_calls"] = nlohmann::json::array();
                    for(const auto& tc : response.tool_calls){
                        assistant_message["tool_calls"].push_back(tc);
                    }
                }
                messages.push_back(assistant_message);
                
                std::cout << "\n\n--- Tool Output ---\n" << std::flush;
                
                for (const auto& tool_call : response.tool_calls) {
                    std::string result;
                    try {
                        std::string tool_name = tool_call.function["name"];
                        auto arguments = nlohmann::json::parse(tool_call.function["arguments"].get<std::string>());
                        std::string code_to_run = unescape_string(arguments["code"]);

                        if (tool_name == "python") {
                            result = python_executor.execute(code_to_run);
                        } else if (tool_name == "powershell" || tool_name == "batch") {
                            result = execute_shell_code(tool_name, code_to_run);
                        } else {
                            result = "Error: Unknown tool '" + tool_name + "'";
                        }

                    } catch (const nlohmann::json::parse_error& e) {
                        result = "Error: Could not decode arguments: " + tool_call.function["arguments"].get<std::string>() + ". Details: " + e.what();
                    } catch (const std::exception& e) {
                        result = "Error: " + std::string(e.what());
                    }

                    std::cout << format_output_for_display(result) << std::flush;
                    std::cout << std::endl;

                    messages.push_back({
                        {"role", "tool"},
                        {"tool_call_id", tool_call.id},
                        {"content", result}
                    });
                }
                continue; // 继续工具调用循环

            } else if (response.type == ApiResponse::Type::MESSAGE) {
                std::cout << std::endl;
                if (!response.content.empty()) {
                    messages.push_back({{"role", "assistant"}, {"content", response.content}});
                }
                break; // 结束工具调用循环，等待用户输入
            }
        }
        std::cout << "\n" << std::endl;
    }
}


int main() {
    // 在 Windows 上设置控制台代码页以正确显示UTF-8字符
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    // 注册信号处理器以处理 Ctrl+C
    signal(SIGINT, signal_handler);

    try {
        main_loop();
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] An unhandled exception occurred: " << e.what() << std::endl;
        if(g_python_executor) {
            delete g_python_executor; // 清理
            g_python_executor = nullptr;
        }
        return 1;
    }

    return 0;
}
