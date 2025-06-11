[English](README.md) | [**简体中文**](README.zh.md)

# Code Atlas

**Code Atlas** 是一个强大的 C++ 应用程序，灵感来源于 [Open Interpreter](https://github.com/OpenInterpreter/open-interpreter)。

作为智能本地代理，它能够直接在您的机器上执行 Python、PowerShell 和批处理脚本。它与大语言模型 (LLM) 集成，提供交互式编程助手，能够理解自然语言请求并执行代码来完成任务。

**目前在 Windows 上开发和测试，Linux 和 macOS 支持正在进行中。**

## ✨ 特性

- **🤖 本地 AI 代理**：完全在您的机器上运行，无需外部 API 依赖
- **💬 云端 AI 可选**：可以选择配置 OpenAI 兼容的 API
- **🐍 多语言执行**：支持 Python、PowerShell 和批处理脚本执行
- **🔄 持久会话**：在多次执行中保持 Python 解释器状态
- **🚀 本地 LLM 集成**：与 llama.cpp 服务器配合，实现快速、私密的 AI 推理
- **⚡ 实时交互**：具有流式响应的交互式命令行界面
- **🛡️ 安全私密**：所有处理都在本地进行 - 您的代码永远不会离开您的机器
- **🔧 可配置**：灵活的基于 JSON 的配置系统
- **🌐 跨平台**：主要支持 Windows，后续将完整支持 Linux/macOS

## 📋 先决条件

### 系统要求
- **操作系统**：Windows 10/11（主要）、Linux 或 macOS
- **CPU**：现代 x64 处理器（推荐支持 CUDA 的 GPU 以获得更快的推理速度）
- **内存**：最少 8GB（推荐 16GB+ 用于较大模型）
- **存储**：至少 10GB 可用空间用于模型和依赖项

### 依赖项
- **CMake** 3.16 或更高版本
- **C++17 兼容编译器**（GCC、Clang 或 MSVC）
- **Python 3.x** 及开发头文件
- **Git**

#### Windows (MSYS2/MinGW64)
```bash
# 从 https://www.msys2.org/ 安装 MSYS2
# 在 mingw64 中执行
pacman -Syu
pacman -Su
# 重启后执行以下操作：
# 安装依赖项：
pacman -S --needed \
    mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-cpr \
    mingw-w64-x86_64-nlohmann-json \
    mingw-w64-x86_64-python
```

## 🚀 快速开始

你可以直接前往 [Realease](https://github.com/ystemsrx/Code-Atlas/releases) 获取我们已编译好的可执行文件。

或者从源码构建：

### 1. 克隆仓库
```bash
git clone --depth 1 https://github.com/ystemsrx/Code-Atlas.git
cd code-atlas
```

### 2. 构建项目

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### 3. 配置应用程序

编辑 `config.json` 以配置您的 LLM 设置：
```json
{
    "api": {
        "base_url": "https://api.openai.com/v1/chat/completions",
        "key": "sk-..."
    },
    "model": {
        "name": "gpt-4o",
        "parameters": {
            "temperature": 0.2,
            "top_p": 0.9,
            "max_tokens": 4096,
            "frequency_penalty": 0.0,
            "presence_penalty": 0.6
        }
    }
}
```

### 4. 启动 LLM 服务器（可选）
如果使用包含的 [llama.cpp](https://github.com/ggml-org/llama.cpp) 服务器：
```bash
llama-server --jinja -fa -m model.gguf

# 或者使用以下命令 （这将从HuggingFace中下载模型）：

llama-server --jinja -fa -hf user/model.gguf
```

请注意，由于涉及函数调用的兼容性，以上命令可能在不同的模型上会有改动，详情见 [llama.cpp/function-calling.md](https://github.com/ggml-org/llama.cpp/blob/master/docs/function-calling.md)

### 5. 运行 Code Atlas
```bash
./code-atlas
```

## 💡 使用示例

### 基本交互

> 计算10的阶乘

![calculate](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_calculate.png?raw=true)

> 列出 Windows 上所有正在运行的进程

![get_process](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_get_process.png?raw=true)

> 创建/重命名文件

![get_process](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_create_files.png?raw=true)

## 🔧 配置

### 配置文件结构

配置文件的模板在根目录的config_template.json中，需复制该模板创建最终生效的 config.json 文件：
```bash
cp config-template.json config.json
```

`config.json` 文件控制 Code Atlas 的所有行为：

```json
{
    "system": {
        "prompt": "You are ..."
    },
    "model": {
        "name": "Model name",
        "parameters": {
            "temperature": 0.2,
            "top_p": 0.9,
            "max_tokens": 4096,
            "frequency_penalty": 0.0,
            "presence_penalty": 0.6
        }
    },
    "api": {
        "base_url": "http://localhost:8080/v1/chat/completions",
        "key": "如果使用云端 API，可能需要填入密钥"
    }
}
```

### 工具配置
Code Atlas 支持三种执行环境：
- **Python**：具有持久状态的完整类 IPython 解释器
- **PowerShell**：Windows PowerShell 脚本执行
- **Batch**：Windows 批处理/CMD 脚本执行

## 🤝 贡献

我们欢迎提出建议和改进，可以通过以下方式参与：

- 提交 Issue
- 提交 Pull Request
- 分享你的使用经验

## 🐛 故障排除

### 常见问题

**构建失败**：
- 确保所有依赖项都已正确安装
- 检查 CMake 版本兼容性（3.16+）
- 验证 Python 开发头文件是否可用

**运行时错误**：
- 检查 `config.json` 语法和路径
- 确保 LLM 服务器正在运行且可访问
- 验证 Python 解释器是否正确嵌入

**性能问题**：
- 考虑为 LLM 推理使用 GPU 加速
- 调整模型参数（temperature、max_tokens）
- 在执行期间监控系统资源

## 📄 许可证

本项目采用 MIT 许可证 - 详情请参阅 [LICENSE](LICENSE) 文件。

## 🙏 致谢

- **llama.cpp** - 提供出色的本地 LLM 推理能力
- **nlohmann/json** - 在 C++ 中提供强大的 JSON 处理
- **cpr** - 提供 HTTP 客户端功能
- **Python** - 提供嵌入式解释器能力

---

**⚠️ 安全提示**：Code Atlas 直接在您的系统上执行代码。仅在受信任的模型和安全环境中使用。在生产环境中执行之前，请始终检查生成的代码。
