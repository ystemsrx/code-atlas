[**English**](README.md) | [简体中文](README.zh.md)

# Code Atlas

**Code Atlas** is a powerful C++ application inspired by [Open Interpreter](https://github.com/OpenInterpreter/open-interpreter).

As an intelligent local agent, it can directly execute Python, PowerShell, and Batch scripts on your machine. Integrated with large language models (LLMs), it provides an interactive programming assistant that understands natural language requests and executes code to accomplish tasks.

**Currently developed and tested on Windows. Linux and macOS support is in progress.**

## ✨ Features

* **🤖 Local AI Agent**: Fully runs on your machine, no external API dependency
* **💬 Cloud AI Optional**: Can optionally connect to OpenAI-compatible APIs
* **🐍 Multi-language Execution**: Supports Python, PowerShell, and Batch scripts
* **🔄 Persistent Sessions**: Maintains Python interpreter state across executions
* **🚀 Local LLM Integration**: Works with `llama.cpp` server for fast and private inference
* **⚡ Real-time Interaction**: Interactive CLI with streaming responses
* **🛡️ Secure & Private**: All processing is local – your code never leaves your machine
* **🔧 Configurable**: Flexible JSON-based configuration system
* **🌐 Cross-platform**: Primarily supports Windows, full Linux/macOS support coming soon

## 📋 Prerequisites

### System Requirements

* **Operating System**: Windows 10/11 (primary), Linux or macOS
* **CPU**: Modern x64 processor (CUDA-capable GPU recommended for faster inference)
* **Memory**: Minimum 8GB (16GB+ recommended for large models)
* **Storage**: At least 10GB free space for models and dependencies

### Dependencies

* **CMake** 3.16 or newer
* **C++17-compatible compiler** (GCC, Clang, or MSVC)
* **Python 3.x** with development headers
* **Git**

#### Windows (MSYS2/MinGW64)

```bash
# Install MSYS2 from https://www.msys2.org/
# Use the mingw64 environment
pacman -Syu
pacman -Su
# After restarting, install dependencies:
pacman -S --needed \
    mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-cpr \
    mingw-w64-x86_64-nlohmann-json \
    mingw-w64-x86_64-python
```

## 🚀 Getting Started

You can directly download prebuilt executables from the [Releases](https://github.com/ystemsrx/Code-Atlas/releases) page.

Or build from source:

### 1. Clone the Repository

```bash
git clone --depth 1 https://github.com/ystemsrx/Code-Atlas.git
cd code-atlas
```

### 2. Build the Project

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### 3. Configure the Application

Edit `config.json` to configure your LLM settings:

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

### 4. Launch LLM Server (Optional)

If you're using the included [llama.cpp](https://github.com/ggml-org/llama.cpp) server:

```bash
llama-server --jinja -fa -m model.gguf

# Or download a model from HuggingFace and run:
llama-server --jinja -fa -hf user/model.gguf
```

> ⚠️ Model-specific differences may apply due to function-calling compatibility. See [llama.cpp/function-calling.md](https://github.com/ggml-org/llama.cpp/blob/master/docs/function-calling.md) for details.

### 5. Run Code Atlas

```bash
./code-atlas
```

## 💡 Usage Examples

### Basic Interactions

> Calculate factorial of 10
> ![calculate](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_calculate.png?raw=true)

> List all running processes on Windows
> ![get_process](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_get_process.png?raw=true)

> Create/rename files
> ![get_process](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_create_files.png?raw=true)

## 🔧 Configuration

### Config File Structure

The config template is located in the root directory as `config_template.json`. Copy it to create your working configuration:

```bash
cp config-template.json config.json
```

The `config.json` file controls all behaviors of Code Atlas:

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
        "key": "Required if using a cloud API"
    }
}
```

### Tool Configuration

Code Atlas supports three execution environments:

* **Python**: A persistent IPython-like interpreter
* **PowerShell**: Native PowerShell script execution on Windows
* **Batch**: Windows CMD/batch script execution

## 🤝 Contributing

We welcome suggestions and contributions. You can participate by:

* Opening Issues
* Submitting Pull Requests
* Sharing your use cases and feedback

## 🐛 Troubleshooting

### Common Issues

**Build Failures**:

* Ensure all dependencies are installed correctly
* Check CMake version compatibility (3.16+)
* Verify Python development headers are available

**Runtime Errors**:

* Check syntax and path of `config.json`
* Ensure LLM server is running and accessible
* Validate Python interpreter embedding is functional

**Performance Issues**:

* Consider GPU acceleration for LLM inference
* Tune model parameters (e.g., `temperature`, `max_tokens`)
* Monitor system resources during execution

## 📄 License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgements

* **llama.cpp** – For robust local LLM inference
* **nlohmann/json** – High-quality JSON parsing for C++
* **cpr** – Simple and effective HTTP client library
* **Python** – Embedded interpreter powering the Python backend

---

**⚠️ Security Warning**:
Code Atlas executes code directly on your system. Use only with trusted models and in secure environments. Always review the generated code before running in production.
