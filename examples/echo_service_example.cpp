/**
 * @file echo_service_example.cpp
 * @brief 简单的 Echo 服务示例
 * 
 * 本示例演示：
 * 1. 创建简单的 RPC 服务
 * 2. 服务端接收字符串并原样返回
 * 3. 客户端调用远程服务
 * 4. 基本的错误处理
 */

#include <frpc/rpc_server.h>
#include <frpc/rpc_client.h>
#include <frpc/config.h>
#include <frpc/logger.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace frpc;

/**
 * @brief Echo 服务实现
 * 
 * 接收一个字符串，原样返回。
 * 
 * @param message 输入消息
 * @return 返回相同的消息
 */
RpcTask<std::string> echo_service(const std::string& message) {
    LOG_INFO("Echo service received: {}", message);
    co_return message;
}

/**
 * @brief 运行服务端
 */
void run_server() {
    try {
        // 配置服务器
        ServerConfig config;
        config.listen_addr = "127.0.0.1";
        config.listen_port = 8080;
        config.max_connections = 100;
        
        // 创建服务器
        RpcServer server(config);
        
        // 注册 echo 服务
        server.register_service("echo", echo_service);
        
        std::cout << "Echo server started on " << config.listen_addr 
                  << ":" << config.listen_port << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // 启动服务器（阻塞）
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

/**
 * @brief 运行客户端
 */
RpcTask<void> run_client() {
    try {
        // 等待服务器启动
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 配置客户端
        ClientConfig config;
        config.default_timeout = std::chrono::milliseconds(5000);
        
        RpcClient client(config);
        
        std::cout << "\n=== Echo Client Started ===" << std::endl;
        
        // 测试消息列表
        std::vector<std::string> messages = {
            "Hello, FRPC!",
            "This is a test message",
            "你好，世界！",
            "123456",
            ""  // 空字符串测试
        };
        
        // 发送每条消息
        for (const auto& msg : messages) {
            std::cout << "\nSending: \"" << msg << "\"" << std::endl;
            
            try {
                auto result = co_await client.call<std::string>("echo", msg);
                
                if (result) {
                    std::cout << "Received: \"" << *result << "\"" << std::endl;
                    
                    // 验证结果
                    if (*result == msg) {
                        std::cout << "✓ Echo successful!" << std::endl;
                    } else {
                        std::cout << "✗ Echo failed: mismatch" << std::endl;
                    }
                } else {
                    std::cout << "✗ No response received" << std::endl;
                }
                
            } catch (const TimeoutException& e) {
                std::cerr << "✗ Timeout: " << e.what() << std::endl;
            } catch (const NetworkException& e) {
                std::cerr << "✗ Network error: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "✗ Error: " << e.what() << std::endl;
            }
            
            // 短暂延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "\n=== All tests completed ===" << std::endl;
        
        // 获取客户端统计信息
        auto stats = client.get_stats();
        std::cout << "\nClient Statistics:" << std::endl;
        std::cout << "  Total calls: " << stats.total_calls << std::endl;
        std::cout << "  Successful: " << stats.successful_calls << std::endl;
        std::cout << "  Failed: " << stats.failed_calls << std::endl;
        std::cout << "  Success rate: " 
                  << (stats.successful_calls * 100.0 / stats.total_calls) 
                  << "%" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // 配置日志
    Logger::set_level(LogLevel::INFO);
    Logger::set_output_console(true);
    
    if (argc > 1 && std::string(argv[1]) == "server") {
        // 运行服务端
        run_server();
    } else if (argc > 1 && std::string(argv[1]) == "client") {
        // 运行客户端
        auto task = run_client();
        // 等待客户端完成...
    } else {
        // 同时运行服务端和客户端（用于演示）
        std::cout << "Starting Echo Service Example..." << std::endl;
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
