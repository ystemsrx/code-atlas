# CMake 最低版本要求
cmake_minimum_required(VERSION 3.16)

# 项目定义
project(code-atlas CXX)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 依赖项查找 ---
# 在 MSYS2 MINGW 环境中，CMake 可以自动找到这些包
# 注意：我们不再需要为 nlohmann_json 指定版本号
find_package(nlohmann_json REQUIRED)
find_package(cpr REQUIRED)

# 查找 Python 解释器和开发库
# MSYS2 的 pacman 会将它们安装在标准路径中
find_package(Python 3 REQUIRED COMPONENTS Interpreter Development)

# --- Windows 特定设置 ---
if(WIN32)
    # 添加用于控制台和文件系统的库
    # 在MinGW下，这个库通常是自动链接的，但显式写上没有坏处
    link_libraries(Shell32)
endif()

# --- 源文件 ---
# 将头文件和源文件分组
file(GLOB HEADERS "include/*.h")
file(GLOB SOURCES "src/*.cpp")

# 添加可执行文件目标
add_executable(
    ${PROJECT_NAME}
    src/main.cpp
    ${HEADERS}
    ${SOURCES}
)

# --- 目标属性和链接 ---
# 为目标添加包含目录
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        ${Python_INCLUDE_DIRS} # 添加 Python 头文件目录
)

# 为目标链接所需的库
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        # 使用 MSYS2/MinGW 时，链接目标名称可能不同
        # CMake 会自动处理 nlohmann_json::nlohmann_json 和 cpr::cpr
        nlohmann_json::nlohmann_json
        cpr::cpr
        ${Python_LIBRARIES} # 链接 Python 库
)
