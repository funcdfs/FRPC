cmake_minimum_required(VERSION 3.20)
project(FRPC VERSION 1.0.0 LANGUAGES CXX)

# C++20 标准要求
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 查找依赖
find_package(Threads REQUIRED)

# 可选：查找 Google Test
find_package(GTest)
if(NOT GTest_FOUND)
    message(STATUS "Google Test not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# 可选：查找 Google Benchmark
find_package(benchmark)
if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# 可选：查找 RapidCheck
find_package(rapidcheck)
if(NOT rapidcheck_FOUND)
    message(STATUS "RapidCheck not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        rapidcheck
        GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
        GIT_TAG master
    )
    set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(rapidcheck)
endif()

# 源文件
file(GLOB_RECURSE FRPC_SOURCES "src/*.cpp")

# 排除平台特定的文件
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(REMOVE_ITEM FRPC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/network_engine.cpp")
    list(REMOVE_ITEM FRPC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/network_awaitable.cpp")
    message(STATUS "Excluding Linux-specific network files on ${CMAKE_SYSTEM_NAME}")
endif()

# FRPC 库
add_library(frpc STATIC ${FRPC_SOURCES})
target_link_libraries(frpc PUBLIC Threads::Threads)

# 为了兼容性，创建 frpc_lib 别名
add_library(frpc_lib ALIAS frpc)

# 测试
enable_testing()
add_subdirectory(tests)

# 性能基准测试
add_subdirectory(benchmarks)

# 压力测试
add_subdirectory(stress_tests)

# 示例
add_subdirectory(examples)

# 文档生成（可选）
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
