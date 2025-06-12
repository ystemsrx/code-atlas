[English](README.md) | [**简体中文**](README.zh.md)

# Code Atlas

**Code Atlas** is a powerful cross-platform local intelligent agent application inspired by [Open Interpreter](https://github.com/OpenInterpreter/open-interpreter). Implemented in C++, it supports running Python and shell scripts locally on Windows, Linux, and macOS, with integrated LLMs for natural language-driven interactive programming.

> **Full cross-platform support** for Windows, Linux, and macOS with automatic OS detection and shell selection.

## ✨ Features

* 🤖 **Local AI Agent**: Runs entirely offline without relying on external APIs
* 💬 **Optional Cloud Access**: Supports remote services compatible with the OpenAI API
* 🐍 **Multi-language Execution**: Supports Python / PowerShell / Batch scripts
* 🔄 **Persistent Session State**: Retains Python environment state across multiple rounds
* 🚀 **Built-in LLM Support**: Compatible with `llama.cpp` for local model inference
* ⚡ **Streaming CLI Interface**: Command line supports real-time output
* 🛡️ **Privacy First**: Everything runs locally, no data leaves your machine
* 🔧 **Highly Configurable**: JSON-based configuration system
* 🌐 **Cross-Platform Design**: Works seamlessly across Windows, Linux, and macOS

## 📋 System Requirements

### Operating System

* **OS**: Windows 10/11, Linux, or macOS
* **CPU**: x64 architecture (CUDA-compatible GPU recommended)
* **Memory**: Minimum 8GB (16GB+ recommended)
* **Storage**: At least 10GB free space

### Required Dependencies

* CMake ≥ 3.16
* C++ Compiler (GCC/Clang/MSVC)
* Python 3.x + development headers
* Git

### Windows (MSYS2 / MinGW64)

```bash
pacman -Syu && pacman -Su
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-cpr \
  mingw-w64-x86_64-nlohmann-json \
  mingw-w64-x86_64-python
```

### Linux Example

```bash
sudo apt update && sudo apt install -y ninja-build
pip3 install --upgrade "conan>=1.60,<2"

mkdir -p build && cd build
conan install .. --build=missing

cmake .. -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build .
```

Or simply run:

```bash
./build.sh
```

## 🚀 Getting Started

Download a precompiled binary from [Releases](https://github.com/ystemsrx/code-atlas/releases).

### Or Build from Source

```bash
git clone --depth 1 https://github.com/ystemsrx/code-atlas.git
cd code-atlas
mkdir build
cd build
cmake ..
cmake --build .
```

### Configure Model and API

Edit the `config.json` file (copy from `config_template.json` if missing):

```bash
cp config_template.json config.json
```

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
      "max_tokens": 4096
    }
  }
}
```

### Launch LLM Server (Optional)

For example, with `llama.cpp`:

```bash
llama-server --jinja -fa -m model.gguf
# or:
llama-server --jinja -fa -hf user/model.gguf
```

> Reference: [llama.cpp/function-calling.md](https://github.com/ggml-org/llama.cpp/blob/master/docs/function-calling.md)

### Start the Application

```bash
./code-atlas
```

## 💡 Usage Demo

Calculate factorial:

![calculate](https://github.com/ystemsrx/code-atlas/blob/master/assets/run_calculate.png?raw=true)

List processes:

![get_process](https://github.com/ystemsrx/code-atlas/blob/master/assets/run_get_process.png?raw=true)

Create/rename files:

![create_files](https://github.com/ystemsrx/code-atlas/blob/master/assets/run_create_files.png?raw=true)

## ⚙️ Configuration Details

Code Atlas uses a `config.json` file:

* `system.prompt`: System prompt string
* `model`: Model parameters
* `api`: API base URL and key (if using cloud models)

Supported runtime environments (automatically selected based on OS):

* Python: Stateful execution, IPython-like
* PowerShell/Batch: For Windows
* Bash: For Linux/macOS

## 🧩 Troubleshooting

* **Build Failure**: Check CMake, Python environment, and dependencies
* **Runtime Errors**: Ensure `config.json` is valid and model/API is accessible
* **Low Performance**: Consider enabling GPU acceleration and adjusting model settings

## 🙌 Contributing

Contributions via issues, pull requests, and feedback are highly welcome!

## 📄 License

This project is licensed under the [MIT License](LICENSE).

## 🙏 Acknowledgments

* [llama.cpp](https://github.com/ggml-org/llama.cpp)
* [nlohmann/json](https://github.com/nlohmann/json)
* [cpr](https://github.com/libcpr/cpr)
* [Python](https://www.python.org)

---

⚠️ **Security Notice**: Code Atlas executes scripts locally. Be cautious when using untrusted models or prompts.

---
