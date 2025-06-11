#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cpr/cpr.h>

// 定义用于工具调用的结构体
struct ToolCall {
    std::string id;
    std::string type;
    nlohmann::json function;

    // 用于序列化为JSON
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ToolCall, id, type, function)
};

// 定义API响应的结构体
struct ApiResponse {
    enum class Type { MESSAGE, TOOL_CALL, API_ERROR };
    Type type;
    std::string content;
    std::vector<ToolCall> tool_calls;
    std::string error_message;
};

class ApiClient {
public:
    /**
     * @brief 构造函数。
     * @param config 从配置文件加载的JSON对象。
     */
    explicit ApiClient(const nlohmann::json& config);

    /**
     * @brief 发送消息到API，并处理流式响应。
     * @param messages 当前的对话历史。这是一个引用，因为函数可能会在内部修改它（尽管当前实现没有）。
     * @return ApiResponse 包含模型响应或工具调用请求。
     */
    ApiResponse send_message(const nlohmann::json& messages);

private:
    std::string url;
    nlohmann::json base_payload;
    cpr::Header headers;
};

#endif // API_CLIENT_H
