#include "ApiClient.h"
#include "Utils.h"
#include <cpr/cpr.h>
#include <iostream>
#include <map>

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
        base_payload["tools"] = config["tools"];
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
        bool in_code_block = false;
        std::string code_buffer;
        std::string pending_escape_buffer;
    };
    std::map<int, PrintingState> tool_calls_printing_state;
    
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

                if (delta.contains("content") && !delta["content"].is_null()) {
                    std::string content = delta["content"];
                    assistant_response_content += content;
                    std::cout << content << std::flush;
                }
                
                if (has_tools && delta.contains("tool_calls")) {
                    auto tool_chunk = delta["tool_calls"][0];
                    int idx = tool_chunk["index"];
                    
                    if (tool_calls_data.find(idx) == tool_calls_data.end()) {
                         tool_calls_data[idx] = {"", "function", {{"name", ""}, {"arguments", ""}}};
                         tool_calls_printing_state[idx] = PrintingState{};
                         if(tool_chunk.contains("function") && tool_chunk["function"].contains("name")){
                            std::cout << "\n--- Running Tool: " << tool_chunk["function"]["name"] << " ---\n" << std::flush;
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
                            state.pending_escape_buffer += args_chunk;

                            if (!state.in_code_block) {
                                std::string start_marker = "{\"code\":\"";
                                size_t start_pos = state.pending_escape_buffer.find(start_marker);
                                if (start_pos != std::string::npos) {
                                    state.in_code_block = true;
                                    state.pending_escape_buffer = state.pending_escape_buffer.substr(start_pos + start_marker.length());
                                }
                            }
                            
                            if (state.in_code_block) {
                                state.code_buffer += state.pending_escape_buffer;
                                state.pending_escape_buffer = "";
                                std::string end_marker = "\""; // End of code is just a quote
                                size_t end_pos = state.code_buffer.rfind(end_marker);
                                if (end_pos != std::string::npos && state.code_buffer.rfind("}") == end_pos + 1){
                                    std::string code_to_print = state.code_buffer.substr(0, end_pos);
                                    std::cout << unescape_string(code_to_print) << std::flush;
                                    state.code_buffer = state.code_buffer.substr(end_pos + end_marker.length());
                                    state.in_code_block = false;
                                } else {
                                     auto [safe_content, remaining] = safe_print_with_escapes(state.code_buffer);
                                     if (!safe_content.empty()) {
                                         std::cout << unescape_string(safe_content) << std::flush;
                                         state.code_buffer = remaining;
                                     }
                                }
                            }
                        }
                    }
                }

                if (chunk["choices"][0].contains("finish_reason") && !chunk["choices"][0]["finish_reason"].is_null()){
                    finish_reason = chunk["choices"][0]["finish_reason"];
                }

            } catch (nlohmann::json::parse_error& e) {
                std::cerr << "\n[Error] JSON Decode Error: " << e.what() << std::endl;
                std::cerr << "[Error] Raw line: " << line << std::endl;
            }
        }
        return true;
    };
    
    cpr::Response response = cpr::Post(cpr::Url{url},
                                     cpr::Body{payload.dump()},
                                     headers,
                                     cpr::Timeout{120000},
                                     cpr::WriteCallback{write_callback});

    if (response.status_code >= 400) {
        final_response.type = ApiResponse::Type::API_ERROR;
        final_response.error_message = "Connection Error: " + response.error.message + "\nDetails: " + response.text;
        return final_response;
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
