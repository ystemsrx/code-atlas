#include "ApiClient.h"
#include "Utils.h"
#include <cpr/cpr.h>
#include <iostream>
#include <map>
#include <algorithm>
#include <curl/curl.h>
#include <stdexcept>
#include <vector>
#include "nlohmann/json.hpp"
#include "Color.h"

ApiClient::ApiClient(const nlohmann::json& config) {
    // 从配置中获取URL
    if (config.contains("api") && config["api"].contains("base_url")) {
        url = config["api"]["base_url"];
    } else {
        throw std::runtime_error("Required configuration not found: api.base_url");
    }

    // 设置请求头
    headers["Content-Type"] = "application/json";
    if (config.contains("api") && config["api"].contains("key") && !config["api"]["key"].is_null()) {
        headers["Authorization"] = "Bearer " + config["api"]["key"].get<std::string>();
    }

    // 构建基础的 payload
    base_payload = nlohmann::json::object();
    base_payload["stream"] = true;
    
    if (config.contains("tools") && !config["tools"].empty()) {
        // Filter tools based on current operating system
        nlohmann::json filtered_tools = nlohmann::json::array();
        auto supported_shells = get_supported_shells();

        for (const auto& tool : config["tools"]) {
            if (tool.contains("function") && tool["function"].contains("name")) {
                std::string tool_name = tool["function"]["name"];

                // Check if this tool is supported on the current OS
                bool is_supported = std::find(supported_shells.begin(), supported_shells.end(), tool_name) != supported_shells.end();

                if (is_supported) {
                    filtered_tools.push_back(tool);
                }
            }
        }

        base_payload["tools"] = filtered_tools;
    }

    if (config.contains("model") && config["model"].contains("name") && !config["model"]["name"].get<std::string>().empty()) {
        base_payload["model"] = config["model"]["name"];
    }

    if (config.contains("model") && config["model"].contains("parameters")) {
        const auto& params = config["model"]["parameters"];
        for (auto const& [key, val] : params.items()) {
            if (key == "temperature" || key == "top_p" || key == "max_tokens" || key == "frequency_penalty" || key == "presence_penalty") {
                base_payload[key] = val;
            }
        }
    }
}

ApiResponse ApiClient::send_message(const nlohmann::json& messages) {
    nlohmann::json payload = base_payload;
    payload["messages"] = messages;

    // --- 流式处理的状态变量 ---
    std::string line_buffer;
    std::string assistant_response_content;
    std::string finish_reason;
    bool has_tools = base_payload.contains("tools");

    // 用于累积完整工具调用数据的状态
    std::map<int, ToolCall> tool_calls_data;
    // 用于实时打印工具代码的状态
    struct PrintingState {
        std::string code_buffer;
        std::string last_printed_code;
        std::string language; // For markdown code blocks
        bool in_code_block = false;
        bool found_final_brace = false;
    };
    std::map<int, PrintingState> tool_calls_printing_state;
    PrintingState printing_state;
    std::string saved_buffer;
    bool is_first_chunk = true;
    
    ApiResponse final_response;

    auto write_callback = [&](const std::string_view& data, intptr_t userdata) -> bool {
        line_buffer.append(data);
        size_t pos;
        while ((pos = line_buffer.find('\n')) != std::string::npos) {
            std::string line = line_buffer.substr(0, pos);
            line_buffer.erase(0, pos + 1);

            if (line.rfind("data: ", 0) != 0) {
                continue;
            }
            std::string data_str = line.substr(6);
            if (data_str.find("[DONE]") != std::string::npos) {
                return true;
            }

            try {
                nlohmann::json chunk = nlohmann::json::parse(data_str);
                auto delta = chunk["choices"][0]["delta"];

                if (is_first_chunk) {
                    bool has_content = delta.contains("content") && delta["content"].is_string() && !delta["content"].get<std::string>().empty();
                    bool has_tools = delta.contains("tool_calls") && !delta["tool_calls"].is_null();
                    if (has_content || has_tools) {
                        std::cout << std::endl;
                        is_first_chunk = false;
                    }
                }

                if (delta.contains("content") && !delta["content"].is_null()) {
                    std::string text_chunk = delta["content"];
                    assistant_response_content += text_chunk;

                    std::string current_buffer = saved_buffer + text_chunk;
                    saved_buffer.clear();

                    size_t start_pos = 0;
                    while(start_pos < current_buffer.length()) {
                        if (!printing_state.in_code_block) {
                            size_t block_start = current_buffer.find("```", start_pos);
                            if (block_start != std::string::npos) {
                                // Print text before the code block
                                std::cout << current_buffer.substr(start_pos, block_start - start_pos);
                                
                                size_t lang_end = current_buffer.find('\n', block_start + 3);
                                if (lang_end != std::string::npos) {
                                    printing_state.language = current_buffer.substr(block_start + 3, lang_end - (block_start + 3));
                                    std::cout << Color::YELLOW; // Start yellow color for code block
                                    start_pos = lang_end + 1;
                                    printing_state.in_code_block = true;
                                } else {
                                    // Incomplete ``` tag, save for next chunk
                                    saved_buffer = current_buffer.substr(block_start);
                                    start_pos = current_buffer.length(); // Exit loop
                                }
                            } else {
                                // No code block start found, print rest of buffer
                                std::cout << current_buffer.substr(start_pos);
                                start_pos = current_buffer.length();
                            }
                        } else { // We are in a code block
                            size_t block_end = current_buffer.find("```", start_pos);
                            if (block_end != std::string::npos) {
                                // Print text inside code block
                                std::cout << current_buffer.substr(start_pos, block_end - start_pos);
                                std::cout << Color::RESET; // End yellow color, no extra newline
                                start_pos = block_end + 3;
                                printing_state.in_code_block = false;
                            } else {
                                // No end of block, print everything and wait for next chunk
                                std::cout << current_buffer.substr(start_pos);
                                start_pos = current_buffer.length();
                            }
                        }
                    }
                }
                
                if (has_tools && delta.contains("tool_calls")) {
                    auto tool_chunk = delta["tool_calls"][0];
                    int idx = tool_chunk["index"];
                    
                    if (tool_calls_data.find(idx) == tool_calls_data.end()) {
                         tool_calls_data[idx] = {"", "function", {{"name", ""}, {"arguments", ""}}};
                         tool_calls_printing_state[idx] = PrintingState{}; // Initialize state
                         if(tool_chunk.contains("function") && tool_chunk["function"].contains("name")){
                            std::cout << "\n--- Tool Call: " << tool_chunk["function"]["name"].get<std::string>() << " ---\n" << std::flush;
                         }
                    }

                    if(tool_chunk.contains("id") && !tool_chunk["id"].is_null()){
                        tool_calls_data[idx].id = tool_chunk["id"];
                    }
                    if(tool_chunk.contains("function")){
                        auto func_chunk = tool_chunk["function"];
                        if(func_chunk.contains("name") && !func_chunk["name"].is_null()){
                             tool_calls_data[idx].function["name"] = func_chunk["name"];
                        }
                        if(func_chunk.contains("arguments") && !func_chunk["arguments"].is_null()){
                            std::string args_chunk = func_chunk["arguments"];
                            tool_calls_data[idx].function["arguments"] = tool_calls_data[idx].function["arguments"].get<std::string>() + args_chunk;
                            
                            // 实时打印代码逻辑
                            auto& state = tool_calls_printing_state[idx];
                            state.code_buffer += args_chunk;

                            if (!state.in_code_block) {
                                // A simple heuristic to detect the start of the code within the JSON argument string.
                                if (state.code_buffer.find("{\"code\":\"") != std::string::npos) {
                                    state.in_code_block = true;
                                    std::cout << Color::LIGHT_PINK;
                                    std::cout << "\n"; // Add an extra newline for tool code
                                }
                            }
                            
                            if (state.in_code_block && !state.found_final_brace) {
                                std::string current_code;
                                
                                try {
                                    nlohmann::json parsed_json = nlohmann::json::parse(state.code_buffer);
                                    if (parsed_json.contains("code") && parsed_json["code"].is_string()) {
                                        current_code = parsed_json["code"];
                                        state.found_final_brace = true;
                                    }
                                } catch (const nlohmann::json::parse_error& e) {
                                    // JSON parsing failed, it's likely incomplete. Try parsing with a temporary closing bracket.
                                    try {
                                        std::string temp_json_str = state.code_buffer + "\"}";
                                        nlohmann::json parsed_json = nlohmann::json::parse(temp_json_str);
                                        if (parsed_json.contains("code") && parsed_json["code"].is_string()) {
                                            current_code = parsed_json["code"];
                                        }
                                    } catch (const nlohmann::json::parse_error& e2) {
                                        current_code = ""; // Still fails, wait for more data.
                                    }
                                }
                                
                                // Print the newly added part of the code
                                if (!current_code.empty() && current_code.length() > state.last_printed_code.length()) {
                                    std::string new_part = current_code.substr(state.last_printed_code.length());
                                    std::cout << new_part << std::flush;
                                    state.last_printed_code = current_code;
                                }
                                
                                if (state.found_final_brace) {
                                    std::cout << Color::RESET;
                                }
                            }
                        }
                    }
                }

                if (chunk["choices"][0].contains("finish_reason") && !chunk["choices"][0]["finish_reason"].is_null()){
                    finish_reason = chunk["choices"][0]["finish_reason"];
                }

            } catch (nlohmann::json::parse_error& e) {
                std::cerr << "\n[JSON Parse Error] " << e.what() << std::endl;
                std::cerr << "Raw data: " << data_str << std::endl;
                std::cerr << "This may indicate that the API server returned an invalid response format" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "\n[Error processing stream data] " << e.what() << std::endl;
                std::cerr << "Data: " << data_str << std::endl;
            }
        }
        return true;
    };
    
    cpr::Response response = cpr::Post(cpr::Url{url},
                                     cpr::Body{payload.dump()},
                                     headers,
                                     cpr::Timeout{120000},
                                     cpr::WriteCallback{write_callback});

    // 检查网络连接错误
    if (response.status_code == 0) {
        final_response.type = ApiResponse::Type::API_ERROR;
        final_response.error_message = "[Network Error] Unable to connect to API server: " + url + 
                                     "\nError details: " + response.error.message;
        return final_response;
    }

    // 检查HTTP错误状态码
    if (response.status_code >= 400) {
        final_response.type = ApiResponse::Type::API_ERROR;
        std::string error_msg = "[API Error] HTTP Status Code: " + std::to_string(response.status_code);
        
        if (!response.error.message.empty()) {
            error_msg += "\nConnection Error: " + response.error.message;
        }
        
        if (!response.text.empty()) {
            error_msg += "\nServer Response: " + response.text;
        } else {
            error_msg += "\nServer returned no response content";
        }
        
        final_response.error_message = error_msg;
        return final_response;
    }
    
    // Ensure color is reset if a markdown code block was left open
    if (printing_state.in_code_block) {
        std::cout << Color::RESET;
    }

    final_response.content = assistant_response_content;
    if (finish_reason == "tool_calls" && has_tools) {
        final_response.type = ApiResponse::Type::TOOL_CALL;
        for (const auto& [idx, call] : tool_calls_data) {
            final_response.tool_calls.push_back(call);
        }
    } else {
        final_response.type = ApiResponse::Type::MESSAGE;
    }

    return final_response;
}
