# Minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Project definition
project(code-atlas CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(nlohmann_json REQUIRED)
find_package(cpr REQUIRED)
find_package(Python 3 REQUIRED COMPONENTS Interpreter Development)

# --- Platform specific configurations ---
if(WIN32)
    # Windows-specific libraries
    link_libraries(Shell32)
elseif(APPLE)
    # macOS-specific configurations
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    if(COREFOUNDATION_LIBRARY)
        link_libraries(${COREFOUNDATION_LIBRARY})
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux-specific configurations
    # Add any Linux-specific libraries here if needed
endif()

# --- Source files ---
file(GLOB HEADERS "include/*.h")
file(GLOB SOURCES "src/*.cpp")

add_executable(
    ${PROJECT_NAME}
    src/main.cpp
    ${HEADERS}
    ${SOURCES}
)

# --- Target configuration ---
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        ${Python_INCLUDE_DIRS}
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        nlohmann_json::nlohmann_json
        cpr::cpr
        ${Python_LIBRARIES}
)

# --- Copy config file to build directory ---
set(SOURCE_CONFIG "${CMAKE_SOURCE_DIR}/config.json")
set(TEMPLATE_CONFIG "${CMAKE_SOURCE_DIR}/config_template.json")
set(DEST_CONFIG "${CMAKE_BINARY_DIR}/config.json")

if(NOT EXISTS "${DEST_CONFIG}")
    if(EXISTS "${SOURCE_CONFIG}")
        file(COPY "${SOURCE_CONFIG}" DESTINATION "${CMAKE_BINARY_DIR}")
    elseif(EXISTS "${TEMPLATE_CONFIG}")
        file(COPY "${TEMPLATE_CONFIG}" DESTINATION "${CMAKE_BINARY_DIR}")
        file(RENAME "${CMAKE_BINARY_DIR}/config_template.json" "${DEST_CONFIG}")
    else()
        message(FATAL_ERROR "No config.json or config_template.json found.")
    endif()
endif()
