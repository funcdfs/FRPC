/**
 * @file test_config.cpp
 * @brief 配置管理单元测试
 * 
 * 测试配置结构的默认值和从文件加载功能。
 */

#include <gtest/gtest.h>
#include "frpc/config.h"
#include <fstream>
#include <filesystem>

using namespace frpc;

/**
 * @brief 测试 ServerConfig 默认值
 * 
 * 验证 ServerConfig 的所有字段都有合理的默认值。
 */
TEST(ConfigTest, ServerConfigDefaults) {
    ServerConfig config;
    
    EXPECT_EQ(config.listen_addr, "0.0.0.0");
    EXPECT_EQ(config.listen_port, 8080);
    EXPECT_EQ(config.max_connections, 10000);
    EXPECT_EQ(config.idle_timeout, std::chrono::seconds(300));
    EXPECT_EQ(config.worker_threads, 1);
}

/**
 * @brief 测试 ClientConfig 默认值
 * 
 * 验证 ClientConfig 的所有字段都有合理的默认值。
 */
TEST(ConfigTest, ClientConfigDefaults) {
    ClientConfig config;
    
    EXPECT_EQ(config.default_timeout, std::chrono::milliseconds(5000));
    EXPECT_EQ(config.max_retries, 3);
    EXPECT_EQ(config.load_balance_strategy, "round_robin");
}

/**
 * @brief 测试 PoolConfig 默认值
 * 
 * 验证 PoolConfig 的所有字段都有合理的默认值。
 */
TEST(ConfigTest, PoolConfigDefaults) {
    PoolConfig config;
    
    EXPECT_EQ(config.min_connections, 1);
    EXPECT_EQ(config.max_connections, 100);
    EXPECT_EQ(config.idle_timeout, std::chrono::seconds(60));
    EXPECT_EQ(config.cleanup_interval, std::chrono::seconds(30));
}

/**
 * @brief 测试 HealthCheckConfig 默认值
 * 
 * 验证 HealthCheckConfig 的所有字段都有合理的默认值。
 */
TEST(ConfigTest, HealthCheckConfigDefaults) {
    HealthCheckConfig config;
    
    EXPECT_EQ(config.interval, std::chrono::seconds(10));
    EXPECT_EQ(config.timeout, std::chrono::seconds(3));
    EXPECT_EQ(config.failure_threshold, 3);
    EXPECT_EQ(config.success_threshold, 2);
}

/**
 * @brief 测试从有效的配置文件加载 ServerConfig
 * 
 * 创建一个有效的 JSON 配置文件，验证能够正确解析所有字段。
 */
TEST(ConfigTest, LoadServerConfigFromValidFile) {
    // 创建临时配置文件
    const std::string config_path = "test_server_config.json";
    std::ofstream file(config_path);
    file << R"({
        "listen_addr": "127.0.0.1",
        "listen_port": 9090,
        "max_connections": 5000,
        "idle_timeout_seconds": 600,
        "worker_threads": 4
    })";
    file.close();
    
    // 加载配置
    auto config = ServerConfig::load_from_file(config_path);
    
    // 验证配置值
    EXPECT_EQ(config.listen_addr, "127.0.0.1");
    EXPECT_EQ(config.listen_port, 9090);
    EXPECT_EQ(config.max_connections, 5000);
    EXPECT_EQ(config.idle_timeout, std::chrono::seconds(600));
    EXPECT_EQ(config.worker_threads, 4);
    
    // 清理
    std::filesystem::remove(config_path);
}

/**
 * @brief 测试从有效的配置文件加载 ClientConfig
 * 
 * 创建一个有效的 JSON 配置文件，验证能够正确解析所有字段。
 */
TEST(ConfigTest, LoadClientConfigFromValidFile) {
    // 创建临时配置文件
    const std::string config_path = "test_client_config.json";
    std::ofstream file(config_path);
    file << R"({
        "default_timeout_ms": 3000,
        "max_retries": 5,
        "load_balance_strategy": "random"
    })";
    file.close();
    
    // 加载配置
    auto config = ClientConfig::load_from_file(config_path);
    
    // 验证配置值
    EXPECT_EQ(config.default_timeout, std::chrono::milliseconds(3000));
    EXPECT_EQ(config.max_retries, 5);
    EXPECT_EQ(config.load_balance_strategy, "random");
    
    // 清理
    std::filesystem::remove(config_path);
}

/**
 * @brief 测试从部分配置文件加载
 * 
 * 创建一个只包含部分字段的配置文件，验证缺失的字段使用默认值。
 */
TEST(ConfigTest, LoadServerConfigFromPartialFile) {
    // 创建只包含部分字段的配置文件
    const std::string config_path = "test_partial_config.json";
    std::ofstream file(config_path);
    file << R"({
        "listen_port": 7070,
        "max_connections": 2000
    })";
    file.close();
    
    // 加载配置
    auto config = ServerConfig::load_from_file(config_path);
    
    // 验证指定的字段
    EXPECT_EQ(config.listen_port, 7070);
    EXPECT_EQ(config.max_connections, 2000);
    
    // 验证未指定的字段使用默认值
    EXPECT_EQ(config.listen_addr, "0.0.0.0");
    EXPECT_EQ(config.idle_timeout, std::chrono::seconds(300));
    EXPECT_EQ(config.worker_threads, 1);
    
    // 清理
    std::filesystem::remove(config_path);
}

/**
 * @brief 测试从不存在的文件加载配置
 * 
 * 验证当配置文件不存在时，返回默认配置。
 * 这符合需求 14.7：配置文件无效时使用默认配置。
 */
TEST(ConfigTest, LoadConfigFromNonExistentFile) {
    const std::string config_path = "non_existent_config.json";
    
    // 加载配置（文件不存在）
    auto server_config = ServerConfig::load_from_file(config_path);
    auto client_config = ClientConfig::load_from_file(config_path);
    
    // 验证返回默认配置
    EXPECT_EQ(server_config.listen_addr, "0.0.0.0");
    EXPECT_EQ(server_config.listen_port, 8080);
    EXPECT_EQ(client_config.default_timeout, std::chrono::milliseconds(5000));
    EXPECT_EQ(client_config.max_retries, 3);
}

/**
 * @brief 测试从无效的 JSON 文件加载配置
 * 
 * 验证当配置文件格式无效时，返回默认配置。
 * 这符合需求 14.7：配置文件无效时使用默认配置。
 */
TEST(ConfigTest, LoadConfigFromInvalidJsonFile) {
    // 创建无效的 JSON 文件
    const std::string config_path = "test_invalid_config.json";
    std::ofstream file(config_path);
    file << "{ invalid json content }";
    file.close();
    
    // 加载配置
    auto server_config = ServerConfig::load_from_file(config_path);
    auto client_config = ClientConfig::load_from_file(config_path);
    
    // 验证返回默认配置
    EXPECT_EQ(server_config.listen_addr, "0.0.0.0");
    EXPECT_EQ(server_config.listen_port, 8080);
    EXPECT_EQ(client_config.default_timeout, std::chrono::milliseconds(5000));
    EXPECT_EQ(client_config.max_retries, 3);
    
    // 清理
    std::filesystem::remove(config_path);
}

/**
 * @brief 测试从空文件加载配置
 * 
 * 验证当配置文件为空时，返回默认配置。
 */
TEST(ConfigTest, LoadConfigFromEmptyFile) {
    // 创建空文件
    const std::string config_path = "test_empty_config.json";
    std::ofstream file(config_path);
    file.close();
    
    // 加载配置
    auto server_config = ServerConfig::load_from_file(config_path);
    auto client_config = ClientConfig::load_from_file(config_path);
    
    // 验证返回默认配置
    EXPECT_EQ(server_config.listen_addr, "0.0.0.0");
    EXPECT_EQ(server_config.listen_port, 8080);
    EXPECT_EQ(client_config.default_timeout, std::chrono::milliseconds(5000));
    EXPECT_EQ(client_config.max_retries, 3);
    
    // 清理
    std::filesystem::remove(config_path);
}

/**
 * @brief 测试配置值的边界情况
 * 
 * 验证配置能够正确处理边界值。
 */
TEST(ConfigTest, ConfigBoundaryValues) {
    // 创建包含边界值的配置文件
    const std::string config_path = "test_boundary_config.json";
    std::ofstream file(config_path);
    file << R"({
        "listen_port": 1,
        "max_connections": 1,
        "idle_timeout_seconds": 1,
        "worker_threads": 0
    })";
    file.close();
    
    // 加载配置
    auto config = ServerConfig::load_from_file(config_path);
    
    // 验证边界值
    EXPECT_EQ(config.listen_port, 1);
    EXPECT_EQ(config.max_connections, 1);
    EXPECT_EQ(config.idle_timeout, std::chrono::seconds(1));
    EXPECT_EQ(config.worker_threads, 0);
    
    // 清理
    std::filesystem::remove(config_path);
}

/**
 * @brief 测试配置值的大数值
 * 
 * 验证配置能够正确处理大数值。
 */
TEST(ConfigTest, ConfigLargeValues) {
    // 创建包含大数值的配置文件
    const std::string config_path = "test_large_config.json";
    std::ofstream file(config_path);
    file << R"({
        "listen_port": 65535,
        "max_connections": 100000,
        "idle_timeout_seconds": 86400,
        "worker_threads": 128
    })";
    file.close();
    
    // 加载配置
    auto config = ServerConfig::load_from_file(config_path);
    
    // 验证大数值
    EXPECT_EQ(config.listen_port, 65535);
    EXPECT_EQ(config.max_connections, 100000);
    EXPECT_EQ(config.idle_timeout, std::chrono::seconds(86400));
    EXPECT_EQ(config.worker_threads, 128);
    
    // 清理
    std::filesystem::remove(config_path);
}
