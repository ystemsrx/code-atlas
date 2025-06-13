#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "Config.h"
#include "ApiClient.h"
#include "CodeExecutor.h"
#include "Utils.h"
#include "Color.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Global pointer for signal handler
PythonExecutor* g_python_executor = nullptr;

void signal_handler(int signum) {
    std::cout << "\n[INFO] Exiting gracefully." << std::endl;
    // Ensure Python interpreter is properly closed before exit
    if (g_python_executor) {
        delete g_python_executor;
        g_python_executor = nullptr;
    }
    exit(signum);
}

void main_loop() {
    // Load configuration
    auto config = load_config();

    // Initialize API client and Python executor
    ApiClient api_client(config);
    PythonExecutor python_executor;
    g_python_executor = &python_executor; // Set global pointer

    std::cout << "Code Atlas started" << std::endl;
    std::cout << "Running on: " << os_to_string(detect_operating_system()) << std::endl;
    std::cout << "API server: " << (config.contains("api") && config["api"].contains("base_url") ?
                  config["api"]["base_url"].get<std::string>() : "Not configured") << std::endl;

    // Display supported shells
    auto supported_shells = get_supported_shells();
    std::cout << "Supported execution environments: ";
    for (size_t i = 0; i < supported_shells.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << supported_shells[i];
    }
    std::cout << std::endl << std::endl;

    // Prepare message history
    nlohmann::json messages = nlohmann::json::array();
    if (config.contains("system") && config["system"].contains("prompt") && !config["system"]["prompt"].get<std::string>().empty()) {
        messages.push_back({
            {"role", "system"},
            {"content", config["system"]["prompt"]}
        });
    }

    // Main loop
    while (true) {
        // Add two newlines for proper spacing and reset color to prevent bleed
        std::cout << std::endl << std::endl << Color::RESET << "> ";
        std::string input;
        std::getline(std::cin, input);
        if (std::cin.eof()) { // Handle Ctrl+D or end of file
             signal_handler(0);
        }

        if (input.empty()) {
            continue;
        }

        messages.push_back({{"role", "user"}, {"content", input}});

        // Tool call loop
        while (true) {
            ApiResponse response = api_client.send_message(messages);

            if (response.type == ApiResponse::Type::API_ERROR) {
                std::cerr << Color::RED << "\nAPI Error: " << response.error_message << Color::RESET << std::endl;
                std::cerr << "\nPlease check:" << std::endl;
                std::cerr << "1. Network connection is working properly" << std::endl;
                std::cerr << "2. API server address is correct" << std::endl;
                std::cerr << "3. API key is valid" << std::endl;
                std::cerr << "4. Server is running" << std::endl;
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
                
                for (const auto& tool_call : response.tool_calls) {
                    std::string result;
                    std::string tool_name = "unknown";
                    try {
                        tool_name = tool_call.function["name"];
                        auto arguments = nlohmann::json::parse(tool_call.function["arguments"].get<std::string>());
                        std::string code_to_run = arguments["code"];

                        // The tool call header and code are now streamed by ApiClient.
                        // We just print the output section header.
                        std::cout << "\n\n--- Output ---\n" << std::endl;

                        if (tool_name == "python") {
                            result = python_executor.execute(code_to_run);
                        } else {
                            // Check if the requested shell is supported on this OS
                            auto supported_shells = get_supported_shells();
                            bool is_supported = std::find(supported_shells.begin(), supported_shells.end(), tool_name) != supported_shells.end();

                            if (is_supported) {
                                result = execute_shell_code(tool_name, code_to_run);
                            } else {
                                nlohmann::json error_json;
                                error_json["status"] = "error";
                                error_json["output"] = "Error: Shell '" + tool_name + "' is not supported on " + os_to_string(detect_operating_system());
                                result = error_json.dump();
                            }
                        }

                    } catch (const nlohmann::json::parse_error& e) {
                        nlohmann::json error_json;
                        error_json["status"] = "error";
                        error_json["output"] = "Error: Could not decode arguments: " + tool_call.function["arguments"].get<std::string>() + ". Details: " + e.what();
                        result = error_json.dump();
                    } catch (const std::exception& e) {
                        nlohmann::json error_json;
                        error_json["status"] = "error";
                        error_json["output"] = "Error: " + std::string(e.what());
                        result = error_json.dump();
                    }

                    try {
                        nlohmann::json result_json = nlohmann::json::parse(result);
                        if (result_json.contains("status") && result_json["status"] == "success") {
                            std::string success_output = result_json["output"];
                            std::cout << Color::GREEN << format_output_for_display(success_output) << Color::RESET << std::endl;
                        } else {
                            std::cout << Color::RED << format_output_for_display(result) << Color::RESET << std::endl;
                        }
                    } catch (const nlohmann::json::parse_error& e) {
                        // If result is not a valid JSON, print as is in red.
                        std::cout << Color::RED << format_output_for_display(result) << Color::RESET << std::endl;
                    }
                    std::cout << "\n--------------" << std::endl;

                    messages.push_back({
                        {"role", "tool"},
                        {"tool_call_id", tool_call.id},
                        {"content", result}
                    });
                }
                
                // Continue tool call loop to get the next assistant message
                continue; 

            } else if (response.type == ApiResponse::Type::MESSAGE) {
                // Newline is handled by ApiClient prepending one
                messages.push_back({{"role", "assistant"}, {"content", response.content}});
                break; // End tool call loop, wait for user input
            }
        }
    }
}

void enable_virtual_terminal_processing() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return;
    }
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    enable_virtual_terminal_processing();
    signal(SIGINT, signal_handler);

    try {
        main_loop();
    } catch (const std::exception& e) {
        std::cerr << Color::RED << "\n[Critical Error] Program encountered unhandled exception: " << e.what() << Color::RESET << std::endl;
        std::cerr << Color::RED << "\nThis may be caused by:" << Color::RESET << std::endl;
        std::cerr << Color::RED << "• Incorrect configuration file format" << Color::RESET << std::endl;
        std::cerr << Color::RED << "• Missing dependencies or incompatible versions" << Color::RESET << std::endl;
        std::cerr << Color::RED << "• Insufficient system resources" << Color::RESET << std::endl;
        std::cerr << Color::RED << "• API server configuration issues" << Color::RESET << std::endl;
        
        if(g_python_executor) {
            delete g_python_executor; // Cleanup
            g_python_executor = nullptr;
        }
        return 1;
    }

    return 0;
}
