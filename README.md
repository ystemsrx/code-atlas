[**English**](README.md) | [简体中文](README.zh.md)

# Code Atlas

**Code Atlas** is a powerful local intelligent agent inspired by [Open Interpreter](https://github.com/OpenInterpreter/open-interpreter). Written in C++, it supports local execution of Python, PowerShell, and batch scripts, and integrates with large language models (LLMs) for natural language-driven interactive programming.

> **Currently optimized for Windows. Linux and macOS support is under active development.**

## ✨ Features

* 🤖 **Local AI Agent**: Runs entirely offline with no external API dependency
* 💬 **Optional Cloud Integration**: Compatible with OpenAI-style APIs for cloud inference
* 🐍 **Multi-language Execution**: Supports Python, PowerShell, and batch scripting
* 🔄 **Persistent Interpreter State**: Maintains Python context across multiple interactions
* 🚀 **Built-in LLM Support**: Local inference enabled via llama.cpp
* ⚡ **Streaming CLI Interface**: Real-time command-line interaction
* 🛡️ **Privacy-First**: Your data never leaves your device
* 🔧 **Highly Configurable**: JSON-based settings and customization
* 🌐 **Cross-platform Design**: Initially Windows-focused, expanding to Linux/macOS

## 📋 System Requirements

### OS & Hardware

* **OS**: Windows 10/11 (preferred), Linux, or macOS
* **CPU**: x64 architecture (CUDA-enabled GPU recommended)
* **RAM**: Minimum 8GB (16GB+ recommended)
* **Disk**: At least 10GB free storage

### Core Dependencies

* CMake ≥ 3.16
* C++17-compliant compiler (GCC / Clang / MSVC)
* Python 3.x with development headers
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

Or run:

```bash
./build.sh
```

## 🚀 Quick Start

Visit the [Releases](https://github.com/ystemsrx/Code-Atlas/releases) page to download a precompiled binary.

### Or Build from Source

```bash
git clone --depth 1 https://github.com/ystemsrx/Code-Atlas.git
cd Code-Atlas
mkdir build && cd build
cmake .. && cmake --build .
```

### Configure API and Model

Copy the config template and edit it:

```bash
cp config_template.json config.json
```

Example `config.json`:

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

To use `llama.cpp`, start the server:

```bash
llama-server --jinja -fa -m model.gguf
# or:
llama-server --jinja -fa -hf user/model.gguf
```

> Reference: [llama.cpp/function-calling.md](https://github.com/ggml-org/llama.cpp/blob/master/docs/function-calling.md)

### Run Code Atlas

```bash
./code-atlas
```

## 💡 Demo

Factorial calculation:

![calculate](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_calculate.png?raw=true)

Process listing:

![get\_process](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_get_process.png?raw=true)

File creation/renaming:

![create\_files](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_create_files.png?raw=true)

## ⚙️ Configuration Overview

All configuration is done through `config.json`:

* `system.prompt`: system prompt for the model
* `model`: LLM parameters
* `api`: base URL and key (if using remote models)

Supported execution environments:

* **Python**: Stateful, IPython-style execution
* **PowerShell**: Windows shell scripting
* **Batch**: Windows command-line scripts

## 🧩 Troubleshooting

* **Build errors**: Verify CMake, Python, and dependencies
* **Runtime issues**: Check `config.json` and LLM/API availability
* **Slow performance**: Enable GPU acceleration or tweak model settings

## 🙌 Contributing

You're welcome to contribute via Issues, Pull Requests, or feedback of any kind!

## 📄 License

Code Atlas is licensed under the [MIT License](LICENSE).

## 🙏 Acknowledgments

* [llama.cpp](https://github.com/ggml-org/llama.cpp)
* [nlohmann/json](https://github.com/nlohmann/json)
* [cpr](https://github.com/libcpr/cpr)
* [Python](https://www.python.org)

---

⚠️ **Security Notice**: Code Atlas runs scripts locally. Be cautious with unknown models or prompt sources.

---
