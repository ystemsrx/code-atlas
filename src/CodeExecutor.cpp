#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "CodeExecutor.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <stdexcept>

// Platform-specific includes for subprocess execution
#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <locale>
#include <codecvt>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#endif

#include "Utils.h"

#ifdef _WIN32
// Windows专用：转换ANSI编码的字符串为UTF-8
std::string convert_ansi_to_utf8(const std::string& ansi_str) {
    if (ansi_str.empty()) return "";
    
    // 首先转换为宽字符
    int wide_size = MultiByteToWideChar(CP_ACP, 0, ansi_str.c_str(), -1, nullptr, 0);
    if (wide_size <= 0) return ansi_str; // 转换失败，返回原字符串
    
    std::wstring wide_str(wide_size - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansi_str.c_str(), -1, &wide_str[0], wide_size);
    
    // 然后转换为UTF-8
    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) return ansi_str; // 转换失败，返回原字符串
    
    std::string utf8_str(utf8_size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, &utf8_str[0], utf8_size, nullptr, nullptr);
    
    return utf8_str;
}

// Windows专用：转换UTF-8编码的字符串为ANSI
std::string utf8_to_ansi(const std::string& utf8_str) {
    if (utf8_str.empty()) return "";
    
    // 首先转换为宽字符
    int wide_size = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
    if (wide_size <= 0) return utf8_str; // 转换失败，返回原字符串
    
    std::wstring wide_str(wide_size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wide_str[0], wide_size);
    
    // 然后转换为ANSI
    int ansi_size = WideCharToMultiByte(CP_ACP, 0, wide_str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (ansi_size <= 0) return utf8_str; // 转换失败，返回原字符串
    
    std::string ansi_str(ansi_size - 1, '\0');
    WideCharToMultiByte(CP_ACP, 0, wide_str.c_str(), -1, &ansi_str[0], ansi_size, nullptr, nullptr);
    
    return ansi_str;
}

// 检查字符串是否为有效的UTF-8
bool is_valid_utf8(const std::string& str) {
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        int bytes_to_read = 0;
        
        if (c < 0x80) {
            bytes_to_read = 1;
        } else if ((c >> 5) == 0x06) {
            bytes_to_read = 2;
        } else if ((c >> 4) == 0x0E) {
            bytes_to_read = 3;
        } else if ((c >> 3) == 0x1E) {
            bytes_to_read = 4;
        } else {
            return false; // 无效的UTF-8起始字节
        }
        
        if (i + bytes_to_read > str.length()) {
            return false; // 字符串长度不足
        }
        
        // 检查后续字节
        for (int j = 1; j < bytes_to_read; j++) {
            unsigned char next_byte = str[i + j];
            if ((next_byte >> 6) != 0x02) {
                return false; // 无效的UTF-8后续字节
            }
        }
        
        i += bytes_to_read;
    }
    return true;
}

// 清理和转换输出字符串为有效UTF-8
std::string sanitize_output_for_utf8(const std::string& output) {
    if (output.empty()) return output;
    
    // 首先检查是否已经是有效的UTF-8
    if (is_valid_utf8(output)) {
        return output;
    }
    
    // 如果不是有效UTF-8，尝试从ANSI转换
    std::string converted = convert_ansi_to_utf8(output);
    if (is_valid_utf8(converted)) {
        return converted;
    }
    
    // 如果转换仍然失败，移除无效字符
    std::string sanitized;
    for (char c : output) {
        if (static_cast<unsigned char>(c) < 0x80) {
            // ASCII字符，安全添加
            sanitized += c;
        } else {
            // 非ASCII字符，用占位符替换
            sanitized += "?";
        }
    }
    
    return sanitized;
}
#endif

// --- PythonExecutor Implementation ---

PythonExecutor::PythonExecutor() : main_module(nullptr), main_dict(nullptr) {
    // 确保Python解释器正确初始化
    if (!Py_IsInitialized()) {
    Py_Initialize();
    }
    
    if (!Py_IsInitialized()) {
        throw std::runtime_error("Failed to initialize Python interpreter.");
    }
    
    // 获取 __main__ 模块和其字典 (全局命名空间)
    main_module = PyImport_AddModule("__main__");
    if (!main_module) {
        throw std::runtime_error("Failed to get __main__ module.");
    }
    main_dict = PyModule_GetDict(main_module);

    // 简化预加载，避免复杂的导入
    const char* pre_run_code =
        "import sys\n"
        "import io\n";
    PyObject* result = PyRun_String(pre_run_code, Py_file_input, main_dict, main_dict);
    if (!result) {
        std::string error = check_python_error();
        // 不抛出异常，只是记录错误
    }
    Py_XDECREF(result);
}

PythonExecutor::~PythonExecutor() {
    if (Py_IsInitialized()) {
        Py_Finalize();
    }
}

std::string PythonExecutor::check_python_error() {
    if (PyErr_Occurred()) {
        PyObject *ptype, *pvalue, *ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
        
        if (ptype == NULL) {
            return "Unknown Python Error: PyErr_Fetch returned NULL ptype.";
        }

        PyObject* traceback_module = PyImport_ImportModule("traceback");
        if (traceback_module != NULL) {
            PyObject* format_exception_func = PyObject_GetAttrString(traceback_module, "format_exception");
            if (format_exception_func && PyCallable_Check(format_exception_func)) {
                PyObject* formatted_exception = PyObject_CallFunctionObjArgs(format_exception_func, ptype, pvalue, ptraceback, NULL);
                if (formatted_exception != NULL) {
                    PyObject* joined_str = PyObject_CallMethod(PyUnicode_FromString(""), "join", "O", formatted_exception);
                    if (joined_str) {
                        PyObject* utf8_bytes = PyUnicode_AsUTF8String(joined_str);
                        if (utf8_bytes) {
                            std::string error_str = PyBytes_AsString(utf8_bytes);
                            Py_DECREF(utf8_bytes);
                            Py_DECREF(joined_str);
                            Py_DECREF(formatted_exception);
                            Py_DECREF(traceback_module);
                            Py_XDECREF(ptype);
                            Py_XDECREF(pvalue);
                            Py_XDECREF(ptraceback);
                            return error_str;
                        }
                        Py_DECREF(joined_str);
                    }
                    Py_DECREF(formatted_exception);
                }
            }
            Py_DECREF(traceback_module);
        }
        // Fallback if traceback module fails
        PyObject* str_exc = PyObject_Repr(pvalue);
        const char* c_str_exc = PyUnicode_AsUTF8(str_exc);
        std::string error_str = c_str_exc;

        Py_XDECREF(str_exc);
        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
        PyErr_Clear();
        return "Error during execution: " + error_str;
    }
    return "";
}


std::string PythonExecutor::execute(const std::string& code) {
    // 处理代码，移除前后空白字符
    std::string trimmed_code = code;
    
    size_t start = trimmed_code.find_first_not_of(" \t\n\r");
    if (start != std::string::npos) {
        trimmed_code = trimmed_code.substr(start);
        size_t end = trimmed_code.find_last_not_of(" \t\n\r");
        if (end != std::string::npos) {
            trimmed_code = trimmed_code.substr(0, end + 1);
        }
    }
    
    if (trimmed_code.empty()) {
        return "[No output]";
    }
    
    // 使用更简单的方法捕获输出
    std::string capture_and_execute = 
        "import sys\n"
        "import io\n"
        "from contextlib import redirect_stdout, redirect_stderr\n"
        "captured_stdout = io.StringIO()\n"
        "captured_stderr = io.StringIO()\n"
        "result_value = None\n"
        "try:\n"
        "    with redirect_stdout(captured_stdout), redirect_stderr(captured_stderr):\n";
    
    // 添加用户代码，适当缩进
    std::istringstream iss(trimmed_code);
    std::string line;
    while (std::getline(iss, line)) {
        capture_and_execute += "        " + line + "\n";
    }
    
    capture_and_execute += 
        "except Exception as e:\n"
        "    captured_stderr.write(str(e) + '\\n')\n";
    
    // 执行包装后的代码
    PyObject* result = PyRun_String(capture_and_execute.c_str(), Py_file_input, main_dict, main_dict);
            if (!result) {
        return "Execution failed: " + check_python_error();
            }
            Py_XDECREF(result);
    
    // 获取捕获的输出
    std::string final_output;
    
    // 获取stdout
    PyObject* stdout_obj = PyRun_String("captured_stdout.getvalue()", Py_eval_input, main_dict, main_dict);
    if (stdout_obj) {
        const char* stdout_str = PyUnicode_AsUTF8(stdout_obj);
        if (stdout_str && strlen(stdout_str) > 0) {
            final_output += stdout_str;
        }
        Py_DECREF(stdout_obj);
    }
    
    // 获取stderr
    PyObject* stderr_obj = PyRun_String("captured_stderr.getvalue()", Py_eval_input, main_dict, main_dict);
    if (stderr_obj) {
        const char* stderr_str = PyUnicode_AsUTF8(stderr_obj);
        if (stderr_str && strlen(stderr_str) > 0) {
            if (!final_output.empty()) final_output += "\n";
            final_output += stderr_str;
        }
        Py_DECREF(stderr_obj);
    }
    
    // 对于简单表达式，尝试获取其值
    if (final_output.empty()) {
        // 检查是否是简单表达式
        if (trimmed_code.find('\n') == std::string::npos && 
            trimmed_code.find('=') == std::string::npos &&
            trimmed_code.find("print") != 0 &&
            trimmed_code.find("import") != 0) {
            
            PyObject* expr_result = PyRun_String(trimmed_code.c_str(), Py_eval_input, main_dict, main_dict);
            if (expr_result) {
                PyObject* repr_obj = PyObject_Repr(expr_result);
                if (repr_obj) {
                    const char* repr_str = PyUnicode_AsUTF8(repr_obj);
                    if (repr_str) {
                        final_output = repr_str;
                    }
                    Py_DECREF(repr_obj);
                }
                Py_DECREF(expr_result);
            } else {
                PyErr_Clear();
            }
        }
    }

    return final_output.empty() ? "[No output]" : final_output;
}

// --- ShellExecutor Implementation (Windows) ---
#ifdef _WIN32
std::string execute_shell_code(const std::string& shell_name, const std::string& code) {
    // 1. 创建临时文件
    namespace fs = std::filesystem;
    fs::path temp_dir = fs::temp_directory_path();
    // 生成唯一文件名
    wchar_t temp_file_w[MAX_PATH];
    if (GetTempFileNameW(temp_dir.c_str(), L"CMD", 0, temp_file_w) == 0) {
        throw std::runtime_error("Could not create temporary file name.");
    }
    fs::path temp_file_path(temp_file_w);
    
    std::string ext = (shell_name == "powershell") ? ".ps1" : ".bat";
    fs::path final_temp_path = temp_file_path;
    final_temp_path.replace_extension(ext);
    
    // 如果扩展名不同，需要重命名文件
    if (temp_file_path != final_temp_path) {
        fs::rename(temp_file_path, final_temp_path);
    }

    try {
        {
            if (shell_name == "batch") {
                // 对于batch文件，使用ANSI编码写入
                std::ofstream temp_file(final_temp_path, std::ios::out);
                if (!temp_file.is_open()){
                     throw std::runtime_error("Could not open temporary file for writing.");
                }
                
                // 不使用BOM，而是设置代码页并使用ANSI编码
                temp_file << "@echo off\n";
                temp_file << "chcp 65001 >nul 2>&1\n"; // 设置代码页为UTF-8
                
                // 将UTF-8代码转换为ANSI再写入
#ifdef _WIN32
                std::string ansi_code = utf8_to_ansi(code);
                temp_file << ansi_code;
#else
                temp_file << code;
#endif
                if (!code.empty() && code.back() != '\n') {
                    temp_file << '\n';
                }
            } else {
                // 对于PowerShell文件，继续使用UTF-8编码
                std::ofstream temp_file(final_temp_path, std::ios::out | std::ios::binary);
                if (!temp_file.is_open()){
                     throw std::runtime_error("Could not open temporary file for writing.");
                }
                
                temp_file << code;
                if (!code.empty() && code.back() != '\n') {
                    temp_file << '\n';
                }
            }
        }

        // 2. 创建用于重定向的匿名管道
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE h_stdout_rd = NULL, h_stdout_wr = NULL;
        HANDLE h_stderr_rd = NULL, h_stderr_wr = NULL;

        if (!CreatePipe(&h_stdout_rd, &h_stdout_wr, &sa, 0)) throw std::runtime_error("Stdout pipe creation failed.");
        if (!SetHandleInformation(h_stdout_rd, HANDLE_FLAG_INHERIT, 0)) throw std::runtime_error("Stdout pipe inheritance setup failed.");
        if (!CreatePipe(&h_stderr_rd, &h_stderr_wr, &sa, 0)) throw std::runtime_error("Stderr pipe creation failed.");
        if (!SetHandleInformation(h_stderr_rd, HANDLE_FLAG_INHERIT, 0)) throw std::runtime_error("Stderr pipe inheritance setup failed.");
        
        // 3. 创建进程
        PROCESS_INFORMATION pi;
        STARTUPINFOW si;
        ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&si, sizeof(STARTUPINFOW));
        si.cb = sizeof(STARTUPINFOW);
        si.hStdError = h_stderr_wr;
        si.hStdOutput = h_stdout_wr;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); // 设置标准输入
        si.dwFlags |= STARTF_USESTDHANDLES;

        std::wstring command_line;
        if (shell_name == "powershell") {
            // 改进PowerShell命令行参数，确保输出不被缓冲
            command_line = L"powershell.exe -ExecutionPolicy Bypass -OutputFormat Text -NonInteractive -File \"" + final_temp_path.wstring() + L"\"";
        } else { // batch
            command_line = L"cmd.exe /c \"" + final_temp_path.wstring() + L"\"";
        }

        if (!CreateProcessW(NULL, &command_line[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(h_stdout_wr); CloseHandle(h_stdout_rd);
            CloseHandle(h_stderr_wr); CloseHandle(h_stderr_rd);
            fs::remove(final_temp_path);
            throw std::runtime_error("CreateProcess failed. Error: " + std::to_string(GetLastError()));
        }

        // 关闭管道的写句柄，以便读取时能收到EOF
        CloseHandle(h_stdout_wr);
        CloseHandle(h_stderr_wr);
        h_stdout_wr = h_stderr_wr = NULL;

        // 4. 改进的输出读取逻辑
        std::string stdout_str, stderr_str;
        DWORD dwRead;
        CHAR chBuf[4096];
        DWORD bytes_available = 0;  // 声明在外层作用域
        
        // 等待进程结束，同时读取输出
        DWORD wait_result;
        bool process_finished = false;
        
        do {
            // 检查进程状态
            wait_result = WaitForSingleObject(pi.hProcess, 100); // 减少等待时间
            process_finished = (wait_result == WAIT_OBJECT_0);
            
            // 读取stdout
            bytes_available = 0;
            if (PeekNamedPipe(h_stdout_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
                if (ReadFile(h_stdout_rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead > 0) {
                    chBuf[dwRead] = '\0';
                    stdout_str += chBuf;
                }
            }
            
            // 读取stderr
            bytes_available = 0;
            if (PeekNamedPipe(h_stderr_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
                if (ReadFile(h_stderr_rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead > 0) {
                    chBuf[dwRead] = '\0';
                    stderr_str += chBuf;
                }
            }
            
        } while (!process_finished || 
                (PeekNamedPipe(h_stdout_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) ||
                (PeekNamedPipe(h_stderr_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0));

        // 最后再读取一次，确保所有数据都被读取
        bytes_available = 0;
        while (PeekNamedPipe(h_stdout_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
            if (ReadFile(h_stdout_rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead > 0) {
                chBuf[dwRead] = '\0';
                stdout_str += chBuf;
            } else {
                break;
            }
        }
        
        bytes_available = 0;
        while (PeekNamedPipe(h_stderr_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
            if (ReadFile(h_stderr_rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead > 0) {
                chBuf[dwRead] = '\0';
                stderr_str += chBuf;
            } else {
                break;
            }
        }
        
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(h_stdout_rd);
        CloseHandle(h_stderr_rd);
        
        fs::remove(final_temp_path);
        
        // 处理输出结果
        std::string result_str;
        if (!stdout_str.empty()) {
            // 对于batch脚本，需要进行编码转换
#ifdef _WIN32
            if (shell_name == "batch") {
                stdout_str = sanitize_output_for_utf8(stdout_str);
            }
#endif
            result_str += stdout_str;
            // 移除末尾的换行符（如果存在）
            while (!result_str.empty() && (result_str.back() == '\n' || result_str.back() == '\r')) {
                result_str.pop_back();
            }
        }
        
        if (!stderr_str.empty()) {
            // 对于batch脚本，需要进行编码转换
#ifdef _WIN32
            if (shell_name == "batch") {
                stderr_str = sanitize_output_for_utf8(stderr_str);
            }
#endif
            if (!result_str.empty()) result_str += "\n";
            result_str += "Error: " + stderr_str;
            // 移除末尾的换行符（如果存在）
            while (!result_str.empty() && (result_str.back() == '\n' || result_str.back() == '\r')) {
                result_str.pop_back();
            }
        }
        
        if (exit_code != 0 && stderr_str.empty()) {
             if (!result_str.empty()) result_str += "\n";
             result_str += "Process exited with code: " + std::to_string(exit_code);
        }
        
        return result_str.empty() ? "[No output]" : result_str;

    } catch (...) {
        fs::remove(final_temp_path); // 确保在异常时也删除文件
        throw;
    }
}
#else
// --- ShellExecutor Implementation (Linux/macOS) ---
std::string execute_shell_code(const std::string& shell_name, const std::string& code) {
    // Improved Linux/macOS implementation with OS-aware shell selection
    namespace fs = std::filesystem;
    fs::path temp_dir = fs::temp_directory_path();

    // Generate unique temporary filename
    std::string temp_filename = "code_exec_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));

    // Determine file extension and command based on shell type
    std::string ext;
    std::string command_prefix;

    if (shell_name == "powershell") {
        ext = ".ps1";
        command_prefix = "pwsh -ExecutionPolicy Bypass -File";
    } else if (shell_name == "bash") {
        ext = ".sh";
        command_prefix = "bash";
    } else {
        // Default to bash for unknown shell types on Unix systems
        ext = ".sh";
        command_prefix = "bash";
    }

    fs::path temp_file_path = temp_dir / (temp_filename + ext);
    
    try {
        // Write to temporary file
        {
            std::ofstream temp_file(temp_file_path, std::ios::out);
            if (!temp_file.is_open()) {
                throw std::runtime_error("Could not create temporary file for shell execution.");
            }

            // Add appropriate shebang for shell scripts
            if (shell_name == "bash" || (shell_name != "powershell" && ext == ".sh")) {
                temp_file << "#!/bin/bash\n";
            }

            // Write code maintaining original format
            // Ensure code ends with newline to avoid execution issues
            temp_file << code;
            if (!code.empty() && code.back() != '\n') {
                temp_file << '\n';
            }
        }
        
        // Set execution permissions
        fs::permissions(temp_file_path, fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec);

        // Build execution command
        std::string command = command_prefix + " \"" + temp_file_path.string() + "\"";
        
        std::array<char, 128> buffer;
        std::string result;
        command += " 2>&1"; // Redirect stderr to stdout

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            fs::remove(temp_file_path);
            throw std::runtime_error("Failed to execute command: " + command);
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        // Clean up temporary file
        fs::remove(temp_file_path);

        // Remove trailing newlines if present
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }

        return result.empty() ? "[No output]" : result;
        
    } catch (...) {
        // Ensure temporary file is deleted even on exception
        fs::remove(temp_file_path);
        throw;
    }
}
#endif
