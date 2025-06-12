[English](README.md) | [**简体中文**](README.zh.md)

# Code Atlas

**Code Atlas** 是一款功能强大的跨平台本地智能代理应用，灵感源自 [Open Interpreter](https://github.com/OpenInterpreter/open-interpreter)，以 C++ 实现，支持在 Windows、Linux 和 macOS 上本地执行 Python 和 shell 脚本，集成 LLM 实现自然语言交互式编程。

> **完整的跨平台支持，包括 Windows、Linux 和 macOS，具有自动操作系统检测和适当的 shell 选择功能。**

## ✨ 主要特性

* 🤖 **本地 AI 代理**：完全脱离外部 API，在本地运行
* 💬 **可选云端接入**：支持接入兼容 OpenAI API 的远程服务
* 🐍 **多语言执行**：支持 Python / PowerShell / 批处理脚本
* 🔄 **持久执行状态**：Python 环境状态跨多轮交互保留
* 🚀 **内置 LLM 支持**：可集成 llama.cpp 实现本地模型推理
* ⚡ **流式交互 CLI**：命令行支持实时输出
* 🛡️ **隐私优先**：本地执行，数据不出机
* 🔧 **高度可配置**：基于 JSON 的配置系统
* 🌐 **跨平台设计**：支持 Windows、Linux 和 macOS

## 📋 安装要求

### 系统需求

* **操作系统**：Windows 10/11、Linux、macOS
* **CPU**：x64 架构，建议具备支持 CUDA 的 GPU
* **内存**：至少 8GB（推荐 16GB+）
* **存储**：10GB 可用空间

### 必要依赖

* CMake ≥ 3.16
* C++ 编译器（GCC/Clang/MSVC）
* Python 3.x + 开发头文件
* Git

### Windows（MSYS2 / MinGW64）

```bash
pacman -Syu && pacman -Su
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-cpr \
  mingw-w64-x86_64-nlohmann-json \
  mingw-w64-x86_64-python
```

### Linux 示例

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

或直接执行：

```bash
./build.sh
```

## 🚀 快速开始

可前往 [Releases](https://github.com/ystemsrx/Code-Atlas/releases) 获取预编译版本。

### 或从源码构建

```bash
git clone --depth 1 https://github.com/ystemsrx/Code-Atlas.git
cd Code-Atlas
mkdir build
cd build
cmake ..
cmake --build .
```

### 配置模型与 API

编辑 `config.json` 文件（可从 `config_template.json` 拷贝）：

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

### 启动 LLM 服务器（可选）

如使用 llama.cpp：

```bash
llama-server --jinja -fa -m model.gguf
# 或：
llama-server --jinja -fa -hf user/model.gguf
```

> 参考：[llama.cpp/function-calling.md](https://github.com/ggml-org/llama.cpp/blob/master/docs/function-calling.md)

### 启动应用

```bash
./code-atlas
```

## 💡 使用演示

计算阶乘：

![calculate](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_calculate.png?raw=true)

列出进程：

![get\_process](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_get_process.png?raw=true)

创建/重命名文件：

![create\_files](https://github.com/ystemsrx/Code-Atlas/blob/master/assets/run_create_files.png?raw=true)

## ⚙️ 配置详情

Code Atlas 配置基于 `config.json`：

* `system.prompt`：系统提示词
* `model`：模型参数
* `api`：API 地址与密钥（如使用云模型）

支持多种执行环境（根据操作系统自动检测）：

* Python：支持状态保持，类 IPython
* PowerShell/Batch：适用于 Windows
* Bash：适用于 Linux/macOS

## 🧩 故障排查

* **构建失败**：检查 CMake / Python 环境及依赖项
* **运行异常**：确认 `config.json` 正确，模型/API 可用
* **性能低**：考虑启用 GPU，调整模型参数

## 🙌 参与贡献

欢迎通过 Issue、PR、反馈经验来参与项目建设！

## 📄 许可证

本项目基于 [MIT License](LICENSE)。

## 🙏 鸣谢

* [llama.cpp](https://github.com/ggml-org/llama.cpp)
* [nlohmann/json](https://github.com/nlohmann/json)
* [cpr](https://github.com/libcpr/cpr)
* [Python](https://www.python.org)

---

⚠️ **安全提示**：Code Atlas 会在本地执行脚本，请谨慎对待来源不明的模型或提示词。

---
