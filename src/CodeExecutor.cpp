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
    
    // 执行用户代码
    PyObject* result = PyRun_String(code.c_str(), Py_file_input, main_dict, main_dict);
    std::string error_output;
    if (!result) {
        error_output = check_python_error();
    }
    Py_XDECREF(result);

    // 获取捕获的输出
    std::string std_output;
    PyObject* output_obj = PyRun_String("redirected_output.getvalue()", Py_eval_input, main_dict, main_dict);
    if(output_obj) {
        std_output = PyUnicode_AsUTF8(output_obj);
        Py_DECREF(output_obj);
    }
    
    PyObject* err_obj = PyRun_String("redirected_error.getvalue()", Py_eval_input, main_dict, main_dict);
    if(err_obj) {
        // Prepend error_output from execute to the captured stderr
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
    if (!std_output.empty()) {
        final_output += std_output;
    }
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
            // 写入BOM以便PowerShell/CMD正确识别UTF-8
            temp_file.put((char)0xEF);
            temp_file.put((char)0xBB);
            temp_file.put((char)0xBF);

            if (shell_name == "batch") {
                temp_file << "@echo off\n";
                temp_file << "chcp 65001 >nul 2>&1\n"; // 设置代码页为UTF-8
            }
            temp_file << code;
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
        si.dwFlags |= STARTF_USESTDHANDLES;

        std::wstring command_line;
        if (shell_name == "powershell") {
            command_line = L"powershell.exe -ExecutionPolicy Bypass -File \"" + final_temp_path.wstring() + L"\"";
        } else { // batch
            command_line = L"cmd.exe /c \"" + final_temp_path.wstring() + L"\"";
        }

        if (!CreateProcessW(NULL, &command_line[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(h_stdout_wr); CloseHandle(h_stdout_rd);
            CloseHandle(h_stderr_wr); CloseHandle(h_stderr_rd);
            fs::remove(final_temp_path);
            throw std::runtime_error("CreateProcess failed. Error: " + std::to_string(GetLastError()));
        }

        // 关闭管道的写句柄，以便读取时能收到EOF
        CloseHandle(h_stdout_wr);
        CloseHandle(h_stderr_wr);
        h_stdout_wr = h_stderr_wr = NULL;

        // 4. 等待进程结束并读取输出
        std::string stdout_str, stderr_str;
        DWORD dwRead;
        CHAR chBuf[4096];

        // 非阻塞读取
        for (;;) {
            bool has_read = false;
            DWORD bytes_available = 0;
            if (PeekNamedPipe(h_stdout_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
                 if (ReadFile(h_stdout_rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead > 0) {
                    chBuf[dwRead] = '\0';
                    stdout_str += chBuf;
                    has_read = true;
                }
            }
             if (PeekNamedPipe(h_stderr_rd, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
                 if (ReadFile(h_stderr_rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead > 0) {
                    chBuf[dwRead] = '\0';
                    stderr_str += chBuf;
                    has_read = true;
                }
            }
            if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0 && !has_read) {
                // If process finished and no more data in pipes, break
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
        
        std::string result_str;
        if (!stdout_str.empty()) result_str += stdout_str;
        if (!stderr_str.empty()) {
            if (!result_str.empty()) result_str += "\n";
            result_str += "Error: " + stderr_str;
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
    // 此处为 Linux/macOS 的实现，使用 popen
    // 注意：popen 无法区分 stdout 和 stderr
    // 一个更完整的实现会使用 fork, exec, pipe
    std::string command;
    if (shell_name == "powershell") {
        command = "pwsh -c '" + code + "'";
    } else { // bash/sh
        command = code;
    }

    std::array<char, 128> buffer;
    std::string result;
    command += " 2>&1"; // 重定向 stderr 到 stdout

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result.empty() ? "[No output]" : result;
}
#endif
