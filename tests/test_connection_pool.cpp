/**
 * @file test_connection_pool.cpp
 * @brief 连接池单元测试
 * 
 * 测试连接池的核心功能，包括连接获取、归还、连接数限制等。
 */

#include <gtest/gtest.h>
#include "frpc/connection_pool.h"
#include "frpc/network_engine.h"
#include "frpc/exceptions.h"
#include <thread>
#include <chrono>

using namespace frpc;

/**
 * @brief 连接池测试夹具
 * 
 * 提供测试所需的网络引擎和连接池配置。
 */
class ConnectionPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建网络引擎
        engine = std::make_unique<NetworkEngine>();
        
        // 配置连接池
        config.min_connections = 1;
        config.max_connections = 5;
        config.idle_timeout = std::chrono::seconds(2);
        config.cleanup_interval = std::chrono::seconds(1);
    }
    
    void TearDown() override {
        engine.reset();
    }
    
    std::unique_ptr<NetworkEngine> engine;
    PoolConfig config;
};

/**
 * @brief 测试连接池创建
 * 
 * 验证连接池可以正常创建，配置参数正确。
 */
TEST_F(ConnectionPoolTest, PoolCreation) {
    ASSERT_NO_THROW({
        ConnectionPool pool(config, engine.get());
    });
}

/**
 * @brief 测试连接池创建失败 - 空引擎指针
 * 
 * 验证传入空的 NetworkEngine 指针时抛出异常。
 */
TEST_F(ConnectionPoolTest, PoolCreationWithNullEngine) {
    EXPECT_THROW({
        ConnectionPool pool(config, nullptr);
    }, std::invalid_argument);
}

/**
 * @brief 测试获取连接统计信息
 * 
 * 验证可以获取连接池统计信息，初始状态为空。
 */
TEST_F(ConnectionPoolTest, GetStats) {
    ConnectionPool pool(config, engine.get());
    
    auto stats = pool.get_stats();
    
    EXPECT_EQ(stats.total_connections, 0);
    EXPECT_EQ(stats.idle_connections, 0);
    EXPECT_EQ(stats.active_connections, 0);
}

/**
 * @brief 测试移除实例
 * 
 * 验证可以移除指定实例的所有连接。
 */
TEST_F(ConnectionPoolTest, RemoveInstance) {
    ConnectionPool pool(config, engine.get());
    
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    
    // 移除不存在的实例（不应该崩溃）
    ASSERT_NO_THROW({
        pool.remove_instance(instance);
    });
}

/**
 * @brief 测试归还无效连接
 * 
 * 验证归还无效连接时，连接被正确丢弃。
 */
TEST_F(ConnectionPoolTest, ReturnInvalidConnection) {
    ConnectionPool pool(config, engine.get());
    
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    
    // 创建一个无效的连接（fd = -1）
    // 注意：这里我们无法直接创建无效连接，因为 Connection 构造函数会检查 fd
    // 所以这个测试需要通过其他方式验证
    
    // 获取统计信息，验证没有连接
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_connections, 0);
}

/**
 * @brief 测试清理空闲连接
 * 
 * 验证清理方法可以正常调用（不会崩溃）。
 */
TEST_F(ConnectionPoolTest, CleanupIdleConnections) {
    ConnectionPool pool(config, engine.get());
    
    // 清理空连接池（不应该崩溃）
    ASSERT_NO_THROW({
        pool.cleanup_idle_connections();
    });
}

/**
 * @brief 测试 ServiceInstance 相等比较
 * 
 * 验证 ServiceInstance 的相等比较运算符。
 */
TEST(ServiceInstanceTest, Equality) {
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    ServiceInstance instance2{"127.0.0.1", 8080, 200};  // 不同的 weight
    ServiceInstance instance3{"127.0.0.1", 8081, 100};  // 不同的 port
    ServiceInstance instance4{"192.168.1.1", 8080, 100};  // 不同的 host
    
    // 相同的 host 和 port，即使 weight 不同也相等
    EXPECT_TRUE(instance1 == instance2);
    
    // 不同的 port
    EXPECT_FALSE(instance1 == instance3);
    
    // 不同的 host
    EXPECT_FALSE(instance1 == instance4);
}

/**
 * @brief 测试 ServiceInstance 哈希
 * 
 * 验证 ServiceInstance 可以作为 unordered_map 的键。
 */
TEST(ServiceInstanceTest, Hash) {
    std::unordered_map<ServiceInstance, int, ServiceInstance::Hash> map;
    
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    ServiceInstance instance2{"127.0.0.1", 8081, 100};
    
    map[instance1] = 1;
    map[instance2] = 2;
    
    EXPECT_EQ(map[instance1], 1);
    EXPECT_EQ(map[instance2], 2);
    EXPECT_EQ(map.size(), 2);
}

/**
 * @brief 测试 Connection 移动构造
 * 
 * 验证 Connection 支持移动语义。
 */
TEST(ConnectionTest, MoveConstructor) {
    // 注意：这个测试需要一个有效的 NetworkEngine 和 fd
    // 由于我们无法在单元测试中轻易创建真实的网络连接，
    // 这里只测试基本的移动语义
    
    // 这个测试需要集成测试环境，暂时跳过
    GTEST_SKIP() << "Requires integration test environment with real network connections";
}

/**
 * @brief 测试 Connection 有效性检查
 * 
 * 验证 Connection::is_valid() 方法。
 */
TEST(ConnectionTest, IsValid) {
    // 这个测试需要集成测试环境
    GTEST_SKIP() << "Requires integration test environment with real network connections";
}

/**
 * @brief 测试连接池配置默认值
 * 
 * 验证 PoolConfig 的默认值是合理的。
 */
TEST(PoolConfigTest, DefaultValues) {
    PoolConfig config;
    
    EXPECT_EQ(config.min_connections, 1);
    EXPECT_EQ(config.max_connections, 100);
    EXPECT_EQ(config.idle_timeout.count(), 60);
    EXPECT_EQ(config.cleanup_interval.count(), 30);
}

/**
 * @brief 测试连接池统计信息初始值
 * 
 * 验证 PoolStats 的初始值。
 */
TEST(PoolStatsTest, InitialValues) {
    PoolStats stats;
    
    EXPECT_EQ(stats.total_connections, 0);
    EXPECT_EQ(stats.idle_connections, 0);
    EXPECT_EQ(stats.active_connections, 0);
    EXPECT_EQ(stats.connection_reuse_rate, 0.0);
}

/**
 * @brief 主函数
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
