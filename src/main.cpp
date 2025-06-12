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
        std::cout << "> ";
        std::string user_input;
        std::getline(std::cin, user_input);
        if (std::cin.eof()) { // Handle Ctrl+D or end of file
             signal_handler(0);
        }
        std::cout << std::endl;

        if (user_input.empty()) {
            continue;
        }

        messages.push_back({{"role", "user"}, {"content", user_input}});

        // Tool call loop
        while (true) {
            ApiResponse response = api_client.send_message(messages);

            if (response.type == ApiResponse::Type::API_ERROR) {
                std::cerr << "\n[API Error] " << response.error_message << std::endl;
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
                
                std::cout << "\n\n--- Tool Output ---\n" << std::flush;
                
                for (const auto& tool_call : response.tool_calls) {
                    std::string result;
                    try {
                        std::string tool_name = tool_call.function["name"];
                        auto arguments = nlohmann::json::parse(tool_call.function["arguments"].get<std::string>());
                        std::string code_to_run = arguments["code"];

                        if (tool_name == "python") {
                            result = python_executor.execute(code_to_run);
                        } else {
                            // Check if the requested shell is supported on this OS
                            auto supported_shells = get_supported_shells();
                            bool is_supported = std::find(supported_shells.begin(), supported_shells.end(), tool_name) != supported_shells.end();

                            if (is_supported) {
                                result = execute_shell_code(tool_name, code_to_run);
                            } else {
                                result = "Error: Shell '" + tool_name + "' is not supported on " + os_to_string(detect_operating_system()) +
                                        ". Supported shells: ";
                                for (size_t i = 0; i < supported_shells.size(); ++i) {
                                    if (i > 0) result += ", ";
                                    result += supported_shells[i];
                                }
                            }
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
                
                // Add an empty line before model output
                std::cout << std::endl;
                continue; // Continue tool call loop

            } else if (response.type == ApiResponse::Type::MESSAGE) {
                std::cout << std::endl;
                if (!response.content.empty()) {
                    messages.push_back({{"role", "assistant"}, {"content", response.content}});
                }
                break; // End tool call loop, wait for user input
            }
        }
        std::cout << "\n" << std::endl;
    }
}


int main() {
    // Set console code page on Windows to properly display UTF-8 characters
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    // Register signal handler to handle Ctrl+C
    signal(SIGINT, signal_handler);

    try {
        main_loop();
    } catch (const std::exception& e) {
        std::cerr << "\n[Critical Error] Program encountered unhandled exception: " << e.what() << std::endl;
        std::cerr << "\nThis may be caused by:" << std::endl;
        std::cerr << "• Incorrect configuration file format" << std::endl;
        std::cerr << "• Missing dependencies or incompatible versions" << std::endl;
        std::cerr << "• Insufficient system resources" << std::endl;
        std::cerr << "• API server configuration issues" << std::endl;
        
        if(g_python_executor) {
            delete g_python_executor; // Cleanup
            g_python_executor = nullptr;
        }
        return 1;
    }

    return 0;
}
