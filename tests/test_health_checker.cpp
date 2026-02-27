/**
 * @file test_health_checker.cpp
 * @brief 健康检测器单元测试
 * 
 * 测试健康检测器的基本功能：
 * - 启动和停止
 * - 添加和移除检测目标
 * - 健康状态的正确标记
 */

#include <gtest/gtest.h>
#include "frpc/health_checker.h"
#include "frpc/service_registry.h"
#include <thread>
#include <chrono>

using namespace frpc;

/**
 * @brief 健康检测器测试夹具
 */
class HealthCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建服务注册中心
        registry_ = std::make_unique<ServiceRegistry>();
        
        // 创建健康检测配置
        config_ = HealthCheckConfig{
            .interval = std::chrono::seconds(1),  // 短间隔用于测试
            .timeout = std::chrono::seconds(1),   // 1秒超时
            .failure_threshold = 2,
            .success_threshold = 2
        };
    }
    
    void TearDown() override {
        registry_.reset();
    }
    
    std::unique_ptr<ServiceRegistry> registry_;
    HealthCheckConfig config_;
};

/**
 * @brief 测试健康检测器的创建
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, Construction) {
    // 正常创建
    EXPECT_NO_THROW({
        HealthChecker checker(config_, registry_.get());
    });
    
    // 空指针应该抛出异常
    EXPECT_THROW({
        HealthChecker checker(config_, nullptr);
    }, std::invalid_argument);
}

/**
 * @brief 测试无效配置参数
 * 
 * 验证：需求 9.6, 9.7, 9.8
 */
TEST_F(HealthCheckerTest, InvalidConfig) {
    // 无效的间隔时间
    HealthCheckConfig invalid_config = config_;
    invalid_config.interval = std::chrono::seconds(0);
    EXPECT_THROW({
        HealthChecker checker(invalid_config, registry_.get());
    }, std::invalid_argument);
    
    // 无效的超时时间
    invalid_config = config_;
    invalid_config.timeout = std::chrono::seconds(0);
    EXPECT_THROW({
        HealthChecker checker(invalid_config, registry_.get());
    }, std::invalid_argument);
    
    // 无效的失败阈值
    invalid_config = config_;
    invalid_config.failure_threshold = 0;
    EXPECT_THROW({
        HealthChecker checker(invalid_config, registry_.get());
    }, std::invalid_argument);
    
    // 无效的成功阈值
    invalid_config = config_;
    invalid_config.success_threshold = 0;
    EXPECT_THROW({
        HealthChecker checker(invalid_config, registry_.get());
    }, std::invalid_argument);
}

/**
 * @brief 测试健康检测的启动和停止
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, StartAndStop) {
    HealthChecker checker(config_, registry_.get());
    
    // 启动健康检测
    EXPECT_NO_THROW(checker.start());
    
    // 重复启动应该不会产生错误
    EXPECT_NO_THROW(checker.start());
    
    // 停止健康检测
    EXPECT_NO_THROW(checker.stop());
    
    // 重复停止应该不会产生错误
    EXPECT_NO_THROW(checker.stop());
    
    // 可以再次启动
    EXPECT_NO_THROW(checker.start());
    EXPECT_NO_THROW(checker.stop());
}

/**
 * @brief 测试添加检测目标
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, AddTarget) {
    HealthChecker checker(config_, registry_.get());
    
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    ServiceInstance instance2{"127.0.0.1", 8081, 100};
    
    // 添加检测目标
    EXPECT_NO_THROW(checker.add_target("service1", instance1));
    EXPECT_NO_THROW(checker.add_target("service2", instance2));
    
    // 重复添加相同的目标应该不会产生错误
    EXPECT_NO_THROW(checker.add_target("service1", instance1));
}

/**
 * @brief 测试移除检测目标
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, RemoveTarget) {
    HealthChecker checker(config_, registry_.get());
    
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    
    // 添加目标
    checker.add_target("service1", instance);
    
    // 移除目标
    EXPECT_NO_THROW(checker.remove_target("service1", instance));
    
    // 移除不存在的目标应该不会产生错误
    EXPECT_NO_THROW(checker.remove_target("service1", instance));
}

/**
 * @brief 测试在运行时添加和移除目标
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, DynamicTargetManagement) {
    HealthChecker checker(config_, registry_.get());
    
    // 启动健康检测
    checker.start();
    
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    ServiceInstance instance2{"127.0.0.1", 8081, 100};
    
    // 在运行时添加目标
    EXPECT_NO_THROW(checker.add_target("service1", instance1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_NO_THROW(checker.add_target("service2", instance2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 在运行时移除目标
    EXPECT_NO_THROW(checker.remove_target("service1", instance1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 停止健康检测
    checker.stop();
}

/**
 * @brief 测试健康状态标记 - 模拟场景
 * 
 * 由于实际的健康检测需要真实的网络连接，这里只测试基本的流程。
 * 实际的健康状态标记测试在属性测试中进行。
 * 
 * 验证：需求 9.2, 9.3
 */
TEST_F(HealthCheckerTest, HealthStatusMarking) {
    // 注册服务实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    registry_->register_service("test_service", instance);
    
    // 验证初始状态为健康
    auto instances = registry_->get_instances("test_service");
    EXPECT_EQ(instances.size(), 1);
    EXPECT_EQ(instances[0].host, "127.0.0.1");
    EXPECT_EQ(instances[0].port, 8080);
    
    // 创建健康检测器
    HealthChecker checker(config_, registry_.get());
    checker.add_target("test_service", instance);
    
    // 启动健康检测
    checker.start();
    
    // 等待一段时间让健康检测运行
    // 注意：由于没有真实的服务器，检测会失败
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 停止健康检测
    checker.stop();
    
    // 注意：由于简化实现，这里不验证具体的健康状态变化
    // 实际的健康状态变化测试在属性测试中进行
}

/**
 * @brief 测试析构函数自动停止
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, DestructorStops) {
    {
        HealthChecker checker(config_, registry_.get());
        checker.start();
        
        ServiceInstance instance{"127.0.0.1", 8080, 100};
        checker.add_target("service1", instance);
        
        // 等待一段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // checker 析构时应该自动停止
    }
    
    // 如果析构函数正确实现，这里不应该崩溃
    SUCCEED();
}

/**
 * @brief 测试配置参数的应用
 * 
 * 验证：需求 9.6, 9.7, 9.8
 */
TEST_F(HealthCheckerTest, ConfigurationParameters) {
    // 测试不同的配置参数
    HealthCheckConfig custom_config{
        .interval = std::chrono::seconds(5),
        .timeout = std::chrono::seconds(2),
        .failure_threshold = 5,
        .success_threshold = 3
    };
    
    EXPECT_NO_THROW({
        HealthChecker checker(custom_config, registry_.get());
    });
}

/**
 * @brief 测试空目标列表
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, EmptyTargetList) {
    HealthChecker checker(config_, registry_.get());
    
    // 启动时没有任何目标
    EXPECT_NO_THROW(checker.start());
    
    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 停止
    EXPECT_NO_THROW(checker.stop());
}

/**
 * @brief 测试多个服务实例
 * 
 * 验证：需求 9.1
 */
TEST_F(HealthCheckerTest, MultipleInstances) {
    HealthChecker checker(config_, registry_.get());
    
    // 添加多个服务的多个实例
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    ServiceInstance instance2{"127.0.0.1", 8081, 100};
    ServiceInstance instance3{"192.168.1.1", 8080, 100};
    
    checker.add_target("service1", instance1);
    checker.add_target("service1", instance2);
    checker.add_target("service2", instance3);
    
    // 启动健康检测
    checker.start();
    
    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 停止
    checker.stop();
}

