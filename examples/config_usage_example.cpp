/**
 * @file config_usage_example.cpp
 * @brief 配置管理使用示例
 * 
 * 演示如何使用 FRPC 框架的配置管理功能。
 */

#include "frpc/config.h"
#include <iostream>

using namespace frpc;

/**
 * @brief 打印服务器配置
 */
void print_server_config(const ServerConfig& config) {
    std::cout << "=== Server Configuration ===" << std::endl;
    std::cout << "Listen Address: " << config.listen_addr << std::endl;
    std::cout << "Listen Port: " << config.listen_port << std::endl;
    std::cout << "Max Connections: " << config.max_connections << std::endl;
    std::cout << "Idle Timeout: " << config.idle_timeout.count() << " seconds" << std::endl;
    std::cout << "Worker Threads: " << config.worker_threads << std::endl;
    std::cout << std::endl;
}

/**
 * @brief 打印客户端配置
 */
void print_client_config(const ClientConfig& config) {
    std::cout << "=== Client Configuration ===" << std::endl;
    std::cout << "Default Timeout: " << config.default_timeout.count() << " ms" << std::endl;
    std::cout << "Max Retries: " << config.max_retries << std::endl;
    std::cout << "Load Balance Strategy: " << config.load_balance_strategy << std::endl;
    std::cout << std::endl;
}

/**
 * @brief 打印连接池配置
 */
void print_pool_config(const PoolConfig& config) {
    std::cout << "=== Connection Pool Configuration ===" << std::endl;
    std::cout << "Min Connections: " << config.min_connections << std::endl;
    std::cout << "Max Connections: " << config.max_connections << std::endl;
    std::cout << "Idle Timeout: " << config.idle_timeout.count() << " seconds" << std::endl;
    std::cout << "Cleanup Interval: " << config.cleanup_interval.count() << " seconds" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief 打印健康检测配置
 */
void print_health_check_config(const HealthCheckConfig& config) {
    std::cout << "=== Health Check Configuration ===" << std::endl;
    std::cout << "Interval: " << config.interval.count() << " seconds" << std::endl;
    std::cout << "Timeout: " << config.timeout.count() << " seconds" << std::endl;
    std::cout << "Failure Threshold: " << config.failure_threshold << std::endl;
    std::cout << "Success Threshold: " << config.success_threshold << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "FRPC Configuration Management Example" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << std::endl;
    
    // 示例 1: 使用默认配置
    std::cout << "Example 1: Using Default Configuration" << std::endl;
    std::cout << "---------------------------------------" << std::endl;
    {
        ServerConfig server_config;
        ClientConfig client_config;
        PoolConfig pool_config;
        HealthCheckConfig health_config;
        
        print_server_config(server_config);
        print_client_config(client_config);
        print_pool_config(pool_config);
        print_health_check_config(health_config);
    }
    
    // 示例 2: 从配置文件加载（如果文件存在）
    std::cout << "Example 2: Loading from Configuration File" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    {
        // 尝试从配置文件加载
        // 如果文件不存在或无效，将使用默认配置
        auto server_config = ServerConfig::load_from_file("examples/server_config.json");
        auto client_config = ClientConfig::load_from_file("examples/client_config.json");
        
        print_server_config(server_config);
        print_client_config(client_config);
    }
    
    // 示例 3: 自定义配置
    std::cout << "Example 3: Custom Configuration" << std::endl;
    std::cout << "--------------------------------" << std::endl;
    {
        ServerConfig server_config;
        server_config.listen_addr = "127.0.0.1";
        server_config.listen_port = 9090;
        server_config.max_connections = 5000;
        server_config.idle_timeout = std::chrono::seconds(600);
        server_config.worker_threads = 8;
        
        ClientConfig client_config;
        client_config.default_timeout = std::chrono::milliseconds(3000);
        client_config.max_retries = 5;
        client_config.load_balance_strategy = "least_connection";
        
        print_server_config(server_config);
        print_client_config(client_config);
    }
    
    // 示例 4: 配置验证
    std::cout << "Example 4: Configuration Validation" << std::endl;
    std::cout << "------------------------------------" << std::endl;
    {
        ServerConfig config;
        
        // 验证配置的合理性
        if (config.listen_port < 1024) {
            std::cout << "Warning: Port " << config.listen_port 
                      << " requires root privileges on Linux" << std::endl;
        }
        
        if (config.max_connections > 50000) {
            std::cout << "Warning: Max connections " << config.max_connections 
                      << " may exceed system limits (check ulimit -n)" << std::endl;
        }
        
        if (config.worker_threads == 0) {
            std::cout << "Info: Worker threads set to 0, using single-threaded mode" << std::endl;
        } else {
            std::cout << "Info: Using " << config.worker_threads << " worker threads" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "Configuration examples completed successfully!" << std::endl;
    
    return 0;
}
