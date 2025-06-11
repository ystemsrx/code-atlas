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
#else
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#endif

// --- PythonExecutor Implementation ---

PythonExecutor::PythonExecutor() : main_module(nullptr), main_dict(nullptr) {
    Py_Initialize();
    if (!Py_IsInitialized()) {
        throw std::runtime_error("Failed to initialize Python interpreter.");
    }
    
    // 获取 __main__ 模块和其字典 (全局命名空间)
    main_module = PyImport_AddModule("__main__");
    if (!main_module) {
        throw std::runtime_error("Failed to get __main__ module.");
    }
    main_dict = PyModule_GetDict(main_module);

    // 预加载库
    const char* pre_run_code =
        "import sys\n"
        "import io\n"
        "try:\n"
        "    import zhplot\n"
        "except ImportError:\n"
        "    pass\n";
    PyObject* result = PyRun_String(pre_run_code, Py_file_input, main_dict, main_dict);
    if (!result) {
        check_python_error(); // 检查并打印预运行代码中的错误
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
    // 处理代码，支持多行代码和复合语句
    std::string trimmed_code = code;
    
    // 移除前后空白字符
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
    
    // 改进的代码分割逻辑，保持缩进结构
    std::vector<std::string> code_blocks;
    
    // 检查是否包含需要缩进的关键字
    bool has_indented_blocks = (trimmed_code.find("try:") != std::string::npos ||
                               trimmed_code.find("if ") != std::string::npos ||
                               trimmed_code.find("for ") != std::string::npos ||
                               trimmed_code.find("while ") != std::string::npos ||
                               trimmed_code.find("def ") != std::string::npos ||
                               trimmed_code.find("class ") != std::string::npos ||
                               trimmed_code.find("with ") != std::string::npos ||
                               trimmed_code.find("except") != std::string::npos ||
                               trimmed_code.find("finally") != std::string::npos ||
                               trimmed_code.find("else:") != std::string::npos ||
                               trimmed_code.find("elif ") != std::string::npos);
    
    if (has_indented_blocks) {
        // 如果包含缩进块，将整个代码作为一个块处理
        code_blocks.push_back(trimmed_code);
    } else {
        // 否则可以安全地按行和分号分割
        std::vector<std::string> lines;
        size_t pos = 0;
        size_t found = 0;
        
        // 按换行符分割
        while ((found = trimmed_code.find('\n', pos)) != std::string::npos) {
            std::string line = trimmed_code.substr(pos, found - pos);
            // 保留空行，因为在某些情况下可能有意义
            lines.push_back(line);
            pos = found + 1;
        }
        // 添加最后一行
        if (pos < trimmed_code.length()) {
            std::string last_line = trimmed_code.substr(pos);
            lines.push_back(last_line);
        }
        
        // 如果只有一行，可能包含分号分隔的语句
        if (lines.size() == 1) {
            std::string single_line = lines[0];
            lines.clear();
            
            // 按分号分割单行
            pos = 0;
            while ((found = single_line.find(';', pos)) != std::string::npos) {
                std::string stmt = single_line.substr(pos, found - pos);
                // 移除前后空白
                size_t stmt_start = stmt.find_first_not_of(" \t\r");
                if (stmt_start != std::string::npos) {
                    stmt = stmt.substr(stmt_start);
                    size_t stmt_end = stmt.find_last_not_of(" \t\r");
                    if (stmt_end != std::string::npos) {
                        stmt = stmt.substr(0, stmt_end + 1);
                    }
                    if (!stmt.empty()) {
                        lines.push_back(stmt);
                    }
                }
                pos = found + 1;
            }
            // 添加最后一个语句
            if (pos < single_line.length()) {
                std::string last_stmt = single_line.substr(pos);
                size_t stmt_start = last_stmt.find_first_not_of(" \t\r");
                if (stmt_start != std::string::npos) {
                    last_stmt = last_stmt.substr(stmt_start);
                    size_t stmt_end = last_stmt.find_last_not_of(" \t\r");
                    if (stmt_end != std::string::npos) {
                        last_stmt = last_stmt.substr(0, stmt_end + 1);
                    }
                    if (!last_stmt.empty()) {
                        lines.push_back(last_stmt);
                    }
                }
            }
        }
        
        // 将每行作为一个独立的代码块（对于简单语句）
        for (const auto& line : lines) {
            if (!line.empty()) {
                // 去除行首行尾空白，但保留必要的缩进结构
                std::string trimmed_line = line;
                size_t line_end = trimmed_line.find_last_not_of(" \t\r");
                if (line_end != std::string::npos) {
                    trimmed_line = trimmed_line.substr(0, line_end + 1);
                }
                if (!trimmed_line.empty()) {
                    code_blocks.push_back(trimmed_line);
                }
            }
        }
    }
    
    if (code_blocks.empty()) {
        return "[No output]";
    }

    // 捕获 stdout 和 stderr
    const char* capture_code =
        "import sys\n"
        "import io\n"
        "old_stdout = sys.stdout\n"
        "old_stderr = sys.stderr\n"
        "redirected_output = io.StringIO()\n"
        "redirected_error = io.StringIO()\n"
        "sys.stdout = redirected_output\n"
        "sys.stderr = redirected_error\n";

    PyObject* presult = PyRun_String(capture_code, Py_file_input, main_dict, main_dict);
    if (!presult) return "Failed to redirect stdio:\n" + check_python_error();
    Py_XDECREF(presult);
    
    std::string expression_result;
    std::string error_output;
    
    // 执行除最后一个代码块外的所有代码
    if (code_blocks.size() > 1) {
        for (size_t i = 0; i < code_blocks.size() - 1; ++i) {
            PyObject* result = PyRun_String(code_blocks[i].c_str(), Py_file_input, main_dict, main_dict);
            if (!result) {
                error_output += check_python_error();
            }
            Py_XDECREF(result);
        }
    }
    
    // 处理最后一个代码块 - 尝试作为表达式执行
    std::string last_block = code_blocks.back();
    bool executed_as_expression = false;
    
    // 检查最后一个代码块是否可能是表达式（只对单行简单语句尝试）
    if (!has_indented_blocks && code_blocks.size() > 0 &&
        last_block.find('\n') == std::string::npos && // 单行
        last_block.find("print") != 0 &&
        last_block.find("import") != 0 &&
        last_block.find("from") != 0 &&
        last_block.find("def") != 0 &&
        last_block.find("class") != 0 &&
        last_block.find("if") != 0 &&
        last_block.find("for") != 0 &&
        last_block.find("while") != 0 &&
        last_block.find("try") != 0 &&
        last_block.find("with") != 0 &&
        last_block.find("=") == std::string::npos &&
        last_block.find("del") != 0 &&
        last_block.find("pass") != 0 &&
        last_block.find("break") != 0 &&
        last_block.find("continue") != 0 &&
        last_block.find("return") != 0 &&
        last_block.find("yield") != 0 &&
        last_block.find("raise") != 0 &&
        last_block.find("assert") != 0) {
        
        // 尝试作为表达式执行
        PyObject* result = PyRun_String(last_block.c_str(), Py_eval_input, main_dict, main_dict);
        if (result) {
            // 获取表达式的返回值
            PyObject* repr_obj = PyObject_Repr(result);
            if (repr_obj) {
                expression_result = PyUnicode_AsUTF8(repr_obj);
                Py_DECREF(repr_obj);
            }
            Py_DECREF(result);
            executed_as_expression = true;
        } else {
            PyErr_Clear(); // 清除错误，稍后尝试作为语句执行
        }
    }
    
    // 如果作为表达式执行失败，则作为语句执行
    if (!executed_as_expression) {
        PyObject* result = PyRun_String(last_block.c_str(), Py_file_input, main_dict, main_dict);
        if (!result) {
            error_output += check_python_error();
        }
        Py_XDECREF(result);
    }

    // 获取捕获的输出
    std::string std_output;
    PyObject* output_obj = PyRun_String("redirected_output.getvalue()", Py_eval_input, main_dict, main_dict);
    if(output_obj) {
        std_output = PyUnicode_AsUTF8(output_obj);
        Py_DECREF(output_obj);
    }
    
    PyObject* err_obj = PyRun_String("redirected_error.getvalue()", Py_eval_input, main_dict, main_dict);
    if(err_obj) {
        error_output = error_output + PyUnicode_AsUTF8(err_obj);
        Py_DECREF(err_obj);
    }

    // 恢复 stdout 和 stderr
    const char* restore_code =
        "sys.stdout = old_stdout\n"
        "sys.stderr = old_stderr\n";
    presult = PyRun_String(restore_code, Py_file_input, main_dict, main_dict);
    Py_XDECREF(presult);
    
    std::string final_output;
    
    // 首先添加print输出
    if (!std_output.empty()) {
        final_output += std_output;
    }
    
    // 然后添加表达式结果
    if (!expression_result.empty()) {
        if (!final_output.empty()) final_output += "\n";
        final_output += expression_result;
    }
    
    // 最后添加错误信息
    if (!error_output.empty()) {
        if (!final_output.empty()) final_output += "\n";
        final_output += error_output;
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
            std::ofstream temp_file(final_temp_path, std::ios::out | std::ios::binary);
            if (!temp_file.is_open()){
                 throw std::runtime_error("Could not open temporary file for writing.");
            }
            
            // PowerShell处理UTF-8时不需要BOM，batch需要
            if (shell_name == "batch") {
                // 写入BOM以便CMD正确识别UTF-8
                temp_file.put((char)0xEF);
                temp_file.put((char)0xBB);
                temp_file.put((char)0xBF);
                temp_file << "@echo off\n";
                temp_file << "chcp 65001 >nul 2>&1\n"; // 设置代码页为UTF-8
            }
            
            // 修复缩进问题：保持原始代码的格式，不进行额外处理
            // 确保代码以换行符结尾，避免最后一行执行问题
            temp_file << code;
            if (!code.empty() && code.back() != '\n') {
                temp_file << '\n';
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
            result_str += stdout_str;
            // 移除末尾的换行符（如果存在）
            while (!result_str.empty() && (result_str.back() == '\n' || result_str.back() == '\r')) {
                result_str.pop_back();
            }
        }
        
        if (!stderr_str.empty()) {
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
    // 改进的Linux/macOS实现，使用临时文件以更好地处理缩进和复杂脚本
    namespace fs = std::filesystem;
    fs::path temp_dir = fs::temp_directory_path();
    
    // 生成唯一的临时文件名
    std::string temp_filename = "code_exec_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
    
    std::string ext;
    if (shell_name == "powershell") {
        ext = ".ps1";
    } else {
        ext = ".sh";
    }
    
    fs::path temp_file_path = temp_dir / (temp_filename + ext);
    
    try {
        // 写入临时文件
        {
            std::ofstream temp_file(temp_file_path, std::ios::out);
            if (!temp_file.is_open()) {
                throw std::runtime_error("Could not create temporary file for shell execution.");
            }
            
            // 为bash脚本添加shebang
            if (shell_name != "powershell") {
                temp_file << "#!/bin/bash\n";
            }
            
            // 修复缩进问题：保持原始代码的格式，不进行额外处理
            // 确保代码以换行符结尾，避免最后一行执行问题
            temp_file << code;
            if (!code.empty() && code.back() != '\n') {
                temp_file << '\n';
            }
        }
        
        // 设置执行权限
        fs::permissions(temp_file_path, fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec);
        
        // 执行命令
        std::string command;
        if (shell_name == "powershell") {
            command = "pwsh -ExecutionPolicy Bypass -File \"" + temp_file_path.string() + "\"";
        } else {
            command = "bash \"" + temp_file_path.string() + "\"";
        }
        
        std::array<char, 128> buffer;
        std::string result;
        command += " 2>&1"; // 重定向 stderr 到 stdout

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            fs::remove(temp_file_path);
            throw std::runtime_error("popen() failed!");
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        
        // 清理临时文件
        fs::remove(temp_file_path);
        
        // 移除末尾的换行符（如果存在）
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        
        return result.empty() ? "[No output]" : result;
        
    } catch (...) {
        // 确保在异常时也删除临时文件
        fs::remove(temp_file_path);
        throw;
    }
}
#endif
