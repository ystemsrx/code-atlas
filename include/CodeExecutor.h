#ifndef CODE_EXECUTOR_H
#define CODE_EXECUTOR_H

#include <string>
#include <stdexcept>
// Forward declare PyObject instead of including Python.h in the header
struct _object;
using PyObject = struct _object;


/**
 * @class PythonExecutor
 * @brief 管理一个持久化的嵌入式Python解释器会话。
 *
 * 这个类初始化一个Python解释器，并允许在同一个全局命名空间中
 * 重复执行代码，从而模拟了原始Python脚本中持久的IPython会话。
 *
 * 注意：这个类的实例不是线程安全的。
 */
class PythonExecutor {
public:
    /**
     * @brief 构造函数。初始化Python解释器并设置环境。
     */
    PythonExecutor();

    /**
     * @brief 析构函数。关闭Python解释器。
     */
    ~PythonExecutor();

    // 禁用拷贝和移动构造/赋值
    PythonExecutor(const PythonExecutor&) = delete;
    PythonExecutor& operator=(const PythonExecutor&) = delete;
    PythonExecutor(PythonExecutor&&) = delete;
    PythonExecutor& operator=(PythonExecutor&&) = delete;

    /**
     * @brief 在持久的Python会话中执行代码。
     * @param code 要执行的Python代码字符串。
     * @return 捕获的stdout和stderr的组合输出。
     */
    std::string execute(const std::string& code);

private:
    PyObject* main_module;
    PyObject* main_dict;

    /**
     * @brief 检查并处理Python C API调用期间发生的任何错误。
     * @return 一个包含格式化后的traceback的字符串；如果无错误则为空。
     */
    std::string check_python_error();
};


/**
 * @brief 执行一个shell命令（如PowerShell或Batch）并捕获其输出。
 * @param shell_name 用于日志记录的shell名称（例如 "powershell", "batch"）。
 * @param code 要执行的脚本代码。
 * @return 捕获的stdout和stderr的组合输出。
 * @throw std::runtime_error 如果进程创建或执行失败。
 */
std::string execute_shell_code(const std::string& shell_name, const std::string& code);


#endif // CODE_EXECUTOR_H
