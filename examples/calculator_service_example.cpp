/**
 * @file calculator_service_example.cpp
 * @brief 计算服务示例 - 演示参数传递和错误处理
 * 
 * 本示例演示：
 * 1. 多个服务函数的注册
 * 2. 不同数据类型的参数传递（整数、浮点数）
 * 3. 业务逻辑错误处理（除零错误）
 * 4. 客户端调用多个服务
 */

#include <frpc/rpc_server.h>
#include <frpc/rpc_client.h>
#include <frpc/config.h>
#include <frpc/logger.h>
#include <frpc/exceptions.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace frpc;

// ============================================================================
// 服务实现
// ============================================================================

/**
 * @brief 加法服务
 */
RpcTask<int> add_service(int a, int b) {
    LOG_INFO("add({}, {}) called", a, b);
    co_return a + b;
}

/**
 * @brief 减法服务
 */
RpcTask<int> subtract_service(int a, int b) {
    LOG_INFO("subtract({}, {}) called", a, b);
    co_return a - b;
}

/**
 * @brief 乘法服务
 */
RpcTask<int> multiply_service(int a, int b) {
    LOG_INFO("multiply({}, {}) called", a, b);
    co_return a * b;
}

/**
 * @brief 除法服务（整数）
 * 
 * @throws std::invalid_argument 如果除数为 0
 */
RpcTask<int> divide_service(int a, int b) {
    LOG_INFO("divide({}, {}) called", a, b);
    
    if (b == 0) {
        LOG_ERROR("Division by zero attempted");
        throw std::invalid_argument("Division by zero");
    }
    
    co_return a / b;
}

/**
 * @brief 浮点除法服务
 * 
 * @throws std::invalid_argument 如果除数为 0
 */
RpcTask<double> divide_double_service(double a, double b) {
    LOG_INFO("divide_double({}, {}) called", a, b);
    
    if (std::abs(b) < 1e-10) {
        LOG_ERROR("Division by zero attempted");
        throw std::invalid_argument("Division by zero");
    }
    
    co_return a / b;
}

/**
 * @brief 幂运算服务
 */
RpcTask<double> power_service(double base, double exponent) {
    LOG_INFO("power({}, {}) called", base, exponent);
    co_return std::pow(base, exponent);
}

/**
 * @brief 平方根服务
 * 
 * @throws std::invalid_argument 如果输入为负数
 */
RpcTask<double> sqrt_service(double value) {
    LOG_INFO("sqrt({}) called", value);
    
    if (value < 0) {
        LOG_ERROR("Square root of negative number attempted");
        throw std::invalid_argument("Cannot compute square root of negative number");
    }
    
    co_return std::sqrt(value);
}

// ============================================================================
// 服务端
// ============================================================================

void run_server() {
    try {
        // 配置服务器
        ServerConfig config;
        config.listen_addr = "127.0.0.1";
        config.listen_port = 8080;
        config.max_connections = 100;
        
        // 创建服务器
        RpcServer server(config);
        
        // 注册所有计算服务
        server.register_service("add", add_service);
        server.register_service("subtract", subtract_service);
        server.register_service("multiply", multiply_service);
        server.register_service("divide", divide_service);
        server.register_service("divide_double", divide_double_service);
        server.register_service("power", power_service);
        server.register_service("sqrt", sqrt_service);
        
        std::cout << "Calculator server started on " << config.listen_addr 
                  << ":" << config.listen_port << std::endl;
        std::cout << "Available services:" << std::endl;
        std::cout << "  - add(int, int) -> int" << std::endl;
        std::cout << "  - subtract(int, int) -> int" << std::endl;
        std::cout << "  - multiply(int, int) -> int" << std::endl;
        std::cout << "  - divide(int, int) -> int" << std::endl;
        std::cout << "  - divide_double(double, double) -> double" << std::endl;
        std::cout << "  - power(double, double) -> double" << std::endl;
        std::cout << "  - sqrt(double) -> double" << std::endl;
        std::cout << "\nPress Ctrl+C to stop" << std::endl;
        
        // 启动服务器
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

// ============================================================================
// 客户端
// ============================================================================

RpcTask<void> run_client() {
    try {
        // 等待服务器启动
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 配置客户端
        ClientConfig config;
        config.default_timeout = std::chrono::milliseconds(5000);
        
        RpcClient client(config);
        
        std::cout << "\n=== Calculator Client Started ===" << std::endl;
        
        // 测试加法
        std::cout << "\n--- Testing Addition ---" << std::endl;
        {
            auto result = co_await client.call<int>("add", 10, 20);
            if (result) {
                std::cout << "10 + 20 = " << *result << std::endl;
                std::cout << ((*result == 30) ? "✓ Correct" : "✗ Wrong") << std::endl;
            }
        }
        
        // 测试减法
        std::cout << "\n--- Testing Subtraction ---" << std::endl;
        {
            auto result = co_await client.call<int>("subtract", 50, 30);
            if (result) {
                std::cout << "50 - 30 = " << *result << std::endl;
                std::cout << ((*result == 20) ? "✓ Correct" : "✗ Wrong") << std::endl;
            }
        }
        
        // 测试乘法
        std::cout << "\n--- Testing Multiplication ---" << std::endl;
        {
            auto result = co_await client.call<int>("multiply", 6, 7);
            if (result) {
                std::cout << "6 × 7 = " << *result << std::endl;
                std::cout << ((*result == 42) ? "✓ Correct" : "✗ Wrong") << std::endl;
            }
        }
        
        // 测试除法（正常情况）
        std::cout << "\n--- Testing Division (Normal) ---" << std::endl;
        {
            auto result = co_await client.call<int>("divide", 100, 5);
            if (result) {
                std::cout << "100 ÷ 5 = " << *result << std::endl;
                std::cout << ((*result == 20) ? "✓ Correct" : "✗ Wrong") << std::endl;
            }
        }
        
        // 测试除法（除零错误）
        std::cout << "\n--- Testing Division by Zero ---" << std::endl;
        {
            try {
                auto result = co_await client.call<int>("divide", 100, 0);
                std::cout << "✗ Should have thrown exception" << std::endl;
            } catch (const ServiceException& e) {
                std::cout << "✓ Caught expected exception: " << e.what() << std::endl;
            }
        }
        
        // 测试浮点除法
        std::cout << "\n--- Testing Double Division ---" << std::endl;
        {
            auto result = co_await client.call<double>("divide_double", 10.0, 3.0);
            if (result) {
                std::cout << "10.0 ÷ 3.0 = " << *result << std::endl;
                std::cout << (std::abs(*result - 3.333333) < 0.001 ? "✓ Correct" : "✗ Wrong") 
                          << std::endl;
            }
        }
        
        // 测试幂运算
        std::cout << "\n--- Testing Power ---" << std::endl;
        {
            auto result = co_await client.call<double>("power", 2.0, 10.0);
            if (result) {
                std::cout << "2^10 = " << *result << std::endl;
                std::cout << ((*result == 1024.0) ? "✓ Correct" : "✗ Wrong") << std::endl;
            }
        }
        
        // 测试平方根（正常情况）
        std::cout << "\n--- Testing Square Root (Normal) ---" << std::endl;
        {
            auto result = co_await client.call<double>("sqrt", 16.0);
            if (result) {
                std::cout << "√16 = " << *result << std::endl;
                std::cout << ((*result == 4.0) ? "✓ Correct" : "✗ Wrong") << std::endl;
            }
        }
        
        // 测试平方根（负数错误）
        std::cout << "\n--- Testing Square Root of Negative Number ---" << std::endl;
        {
            try {
                auto result = co_await client.call<double>("sqrt", -1.0);
                std::cout << "✗ Should have thrown exception" << std::endl;
            } catch (const ServiceException& e) {
                std::cout << "✓ Caught expected exception: " << e.what() << std::endl;
            }
        }
        
        // 复杂计算示例
        std::cout << "\n--- Complex Calculation Example ---" << std::endl;
        std::cout << "Computing: (10 + 20) × 3 - 50 ÷ 2" << std::endl;
        {
            auto sum = co_await client.call<int>("add", 10, 20);
            auto product = co_await client.call<int>("multiply", *sum, 3);
            auto quotient = co_await client.call<int>("divide", 50, 2);
            auto result = co_await client.call<int>("subtract", *product, *quotient);
            
            std::cout << "Result: " << *result << std::endl;
            std::cout << "Expected: 65" << std::endl;
            std::cout << ((*result == 65) ? "✓ Correct" : "✗ Wrong") << std::endl;
        }
        
        std::cout << "\n=== All tests completed ===" << std::endl;
        
        // 显示统计信息
        auto stats = client.get_stats();
        std::cout << "\nClient Statistics:" << std::endl;
        std::cout << "  Total calls: " << stats.total_calls << std::endl;
        std::cout << "  Successful: " << stats.successful_calls << std::endl;
        std::cout << "  Failed: " << stats.failed_calls << std::endl;
        std::cout << "  Success rate: " 
                  << (stats.successful_calls * 100.0 / stats.total_calls) 
                  << "%" << std::endl;
        std::cout << "  Average latency: " << stats.avg_latency_ms << "ms" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char* argv[]) {
    // 配置日志
    Logger::set_level(LogLevel::INFO);
    Logger::set_output_console(true);
    
    if (argc > 1 && std::string(argv[1]) == "server") {
        // 仅运行服务端
        run_server();
    } else if (argc > 1 && std::string(argv[1]) == "client") {
        // 仅运行客户端
        auto task = run_client();
        // 等待完成...
    } else {
        // 演示模式：同时运行服务端和客户端
        std::cout << "Starting Calculator Service Example..." << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "  " << argv[0] << " server  - Run server only" << std::endl;
        std::cout << "  " << argv[0] << " client  - Run client only" << std::endl;
        std::cout << "  " << argv[0] << "         - Run both (demo mode)" << std::endl;
        std::cout << std::endl;
        
        // 在单独的线程中运行服务器
        std::thread server_thread(run_server);
        server_thread.detach();
        
        // 在主线程运行客户端
        auto task = run_client();
        // 等待完成...
    }
    
    return 0;
}
