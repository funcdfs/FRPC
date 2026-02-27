/**
 * @file test_service_registry.cpp
 * @brief 服务注册中心单元测试
 * 
 * 测试服务注册中心的核心功能，包括服务注册、查询、注销、
 * 多实例注册和健康状态更新。
 * 
 * 验证需求: 8.1, 8.2, 8.3, 8.6
 */

#include <gtest/gtest.h>
#include "frpc/service_registry.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace frpc;

/**
 * @brief 服务注册中心测试夹具
 * 
 * 提供测试所需的服务注册中心和测试数据。
 */
class ServiceRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<ServiceRegistry>();
    }
    
    void TearDown() override {
        registry.reset();
    }
    
    std::unique_ptr<ServiceRegistry> registry;
};

// ============================================================================
// 基础功能测试 - 服务注册和查询
// ============================================================================

/**
 * @brief 测试服务注册和查询
 * 
 * 验证需求 8.1, 8.2: 服务实例可以注册到注册中心，并通过查询获取。
 * 
 * 测试步骤：
 * 1. 注册一个服务实例
 * 2. 查询该服务的实例列表
 * 3. 验证返回的实例列表包含注册的实例
 */
TEST_F(ServiceRegistryTest, RegisterAndQueryService) {
    // 创建服务实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    
    // 注册服务
    bool success = registry->register_service("user_service", instance);
    ASSERT_TRUE(success);
    
    // 查询服务实例
    auto instances = registry->get_instances("user_service");
    
    // 验证返回的实例列表
    ASSERT_EQ(instances.size(), 1);
    EXPECT_EQ(instances[0].host, "127.0.0.1");
    EXPECT_EQ(instances[0].port, 8080);
    EXPECT_EQ(instances[0].weight, 100);
}

/**
 * @brief 测试查询不存在的服务
 * 
 * 验证需求 8.1: 查询不存在的服务应返回空列表。
 */
TEST_F(ServiceRegistryTest, QueryNonExistentService) {
    // 查询不存在的服务
    auto instances = registry->get_instances("non_existent_service");
    
    // 验证返回空列表
    EXPECT_TRUE(instances.empty());
}

/**
 * @brief 测试注册多个不同服务
 * 
 * 验证需求 8.1: 可以注册多个不同的服务。
 */
TEST_F(ServiceRegistryTest, RegisterMultipleServices) {
    // 注册多个服务
    ServiceInstance user_instance{"127.0.0.1", 8080, 100};
    ServiceInstance order_instance{"127.0.0.1", 8081, 100};
    ServiceInstance payment_instance{"127.0.0.1", 8082, 100};
    
    registry->register_service("user_service", user_instance);
    registry->register_service("order_service", order_instance);
    registry->register_service("payment_service", payment_instance);
    
    // 查询各个服务
    auto user_instances = registry->get_instances("user_service");
    auto order_instances = registry->get_instances("order_service");
    auto payment_instances = registry->get_instances("payment_service");
    
    // 验证每个服务都有一个实例
    EXPECT_EQ(user_instances.size(), 1);
    EXPECT_EQ(order_instances.size(), 1);
    EXPECT_EQ(payment_instances.size(), 1);
    
    // 验证实例信息正确
    EXPECT_EQ(user_instances[0].port, 8080);
    EXPECT_EQ(order_instances[0].port, 8081);
    EXPECT_EQ(payment_instances[0].port, 8082);
}

// ============================================================================
// 服务注销测试
// ============================================================================

/**
 * @brief 测试服务注销
 * 
 * 验证需求 8.3: 服务实例可以从注册中心注销。
 * 
 * 测试步骤：
 * 1. 注册一个服务实例
 * 2. 注销该实例
 * 3. 查询服务，验证实例已被移除
 */
TEST_F(ServiceRegistryTest, UnregisterService) {
    // 注册服务实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance);
    
    // 验证实例已注册
    auto instances_before = registry->get_instances("user_service");
    ASSERT_EQ(instances_before.size(), 1);
    
    // 注销服务实例
    registry->unregister_service("user_service", instance);
    
    // 查询服务，应该返回空列表
    auto instances_after = registry->get_instances("user_service");
    EXPECT_TRUE(instances_after.empty());
}

/**
 * @brief 测试注销不存在的服务
 * 
 * 验证需求 8.3: 注销不存在的服务不应导致错误。
 */
TEST_F(ServiceRegistryTest, UnregisterNonExistentService) {
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    
    // 注销不存在的服务（不应该崩溃）
    ASSERT_NO_THROW({
        registry->unregister_service("non_existent_service", instance);
    });
}

/**
 * @brief 测试注销不存在的实例
 * 
 * 验证需求 8.3: 注销不存在的实例不应导致错误。
 */
TEST_F(ServiceRegistryTest, UnregisterNonExistentInstance) {
    // 注册一个实例
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance1);
    
    // 尝试注销另一个不存在的实例
    ServiceInstance instance2{"127.0.0.1", 8081, 100};
    
    ASSERT_NO_THROW({
        registry->unregister_service("user_service", instance2);
    });
    
    // 验证原实例仍然存在
    auto instances = registry->get_instances("user_service");
    ASSERT_EQ(instances.size(), 1);
    EXPECT_EQ(instances[0].port, 8080);
}

/**
 * @brief 测试部分注销
 * 
 * 验证需求 8.3: 注销一个实例不影响其他实例。
 */
TEST_F(ServiceRegistryTest, PartialUnregister) {
    // 注册多个实例
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    ServiceInstance instance2{"127.0.0.1", 8081, 100};
    ServiceInstance instance3{"127.0.0.1", 8082, 100};
    
    registry->register_service("user_service", instance1);
    registry->register_service("user_service", instance2);
    registry->register_service("user_service", instance3);
    
    // 验证有 3 个实例
    auto instances_before = registry->get_instances("user_service");
    ASSERT_EQ(instances_before.size(), 3);
    
    // 注销中间的实例
    registry->unregister_service("user_service", instance2);
    
    // 验证剩余 2 个实例
    auto instances_after = registry->get_instances("user_service");
    ASSERT_EQ(instances_after.size(), 2);
    
    // 验证剩余的实例是正确的
    bool has_instance1 = false;
    bool has_instance3 = false;
    for (const auto& inst : instances_after) {
        if (inst.port == 8080) has_instance1 = true;
        if (inst.port == 8082) has_instance3 = true;
    }
    EXPECT_TRUE(has_instance1);
    EXPECT_TRUE(has_instance3);
}

// ============================================================================
// 多实例注册测试
// ============================================================================

/**
 * @brief 测试多实例注册相同服务名称
 * 
 * 验证需求 8.6: 多个实例可以注册相同的服务名称。
 * 
 * 测试步骤：
 * 1. 注册多个实例到同一个服务名称
 * 2. 查询服务，验证返回所有实例
 */
TEST_F(ServiceRegistryTest, MultipleInstancesSameService) {
    // 注册多个实例到同一个服务
    ServiceInstance instance1{"192.168.1.1", 8080, 100};
    ServiceInstance instance2{"192.168.1.2", 8080, 100};
    ServiceInstance instance3{"192.168.1.3", 8080, 100};
    
    registry->register_service("user_service", instance1);
    registry->register_service("user_service", instance2);
    registry->register_service("user_service", instance3);
    
    // 查询服务实例
    auto instances = registry->get_instances("user_service");
    
    // 验证返回所有 3 个实例
    ASSERT_EQ(instances.size(), 3);
    
    // 验证每个实例都存在
    std::vector<std::string> hosts;
    for (const auto& inst : instances) {
        hosts.push_back(inst.host);
    }
    
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), "192.168.1.1") != hosts.end());
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), "192.168.1.2") != hosts.end());
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), "192.168.1.3") != hosts.end());
}

/**
 * @brief 测试重复注册相同实例
 * 
 * 验证需求 8.2: 重复注册相同实例应更新实例信息。
 */
TEST_F(ServiceRegistryTest, RegisterDuplicateInstance) {
    // 注册实例（权重 100）
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance1);
    
    // 验证实例已注册
    auto instances_before = registry->get_instances("user_service");
    ASSERT_EQ(instances_before.size(), 1);
    EXPECT_EQ(instances_before[0].weight, 100);
    
    // 重复注册相同实例（不同权重）
    ServiceInstance instance2{"127.0.0.1", 8080, 200};
    registry->register_service("user_service", instance2);
    
    // 验证仍然只有一个实例，但权重已更新
    auto instances_after = registry->get_instances("user_service");
    ASSERT_EQ(instances_after.size(), 1);
    EXPECT_EQ(instances_after[0].host, "127.0.0.1");
    EXPECT_EQ(instances_after[0].port, 8080);
    EXPECT_EQ(instances_after[0].weight, 200);
}

/**
 * @brief 测试大量实例注册
 * 
 * 验证需求 8.6: 可以注册大量实例到同一个服务。
 */
TEST_F(ServiceRegistryTest, ManyInstancesRegistration) {
    const int num_instances = 100;
    
    // 注册大量实例
    for (int i = 0; i < num_instances; ++i) {
        ServiceInstance instance{
            "192.168.1." + std::to_string(i % 256),
            static_cast<uint16_t>(8000 + i),
            100
        };
        registry->register_service("user_service", instance);
    }
    
    // 查询服务实例
    auto instances = registry->get_instances("user_service");
    
    // 验证所有实例都已注册
    EXPECT_EQ(instances.size(), num_instances);
}

// ============================================================================
// 健康状态更新测试
// ============================================================================

/**
 * @brief 测试健康状态更新
 * 
 * 验证需求 8.6: 可以更新实例的健康状态。
 * 
 * 测试步骤：
 * 1. 注册一个服务实例
 * 2. 将实例标记为不健康
 * 3. 查询服务，验证不健康的实例不在返回列表中
 */
TEST_F(ServiceRegistryTest, UpdateHealthStatus) {
    // 注册服务实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance);
    
    // 验证实例在健康列表中
    auto instances_healthy = registry->get_instances("user_service");
    ASSERT_EQ(instances_healthy.size(), 1);
    
    // 将实例标记为不健康
    registry->update_health_status("user_service", instance, false);
    
    // 查询服务，不健康的实例不应该返回
    auto instances_unhealthy = registry->get_instances("user_service");
    EXPECT_TRUE(instances_unhealthy.empty());
    
    // 将实例标记为健康
    registry->update_health_status("user_service", instance, true);
    
    // 查询服务，实例应该重新出现
    auto instances_recovered = registry->get_instances("user_service");
    ASSERT_EQ(instances_recovered.size(), 1);
    EXPECT_EQ(instances_recovered[0].host, "127.0.0.1");
    EXPECT_EQ(instances_recovered[0].port, 8080);
}

/**
 * @brief 测试部分实例不健康
 * 
 * 验证需求 8.6: get_instances() 只返回健康的实例。
 */
TEST_F(ServiceRegistryTest, PartialUnhealthyInstances) {
    // 注册多个实例
    ServiceInstance instance1{"192.168.1.1", 8080, 100};
    ServiceInstance instance2{"192.168.1.2", 8080, 100};
    ServiceInstance instance3{"192.168.1.3", 8080, 100};
    
    registry->register_service("user_service", instance1);
    registry->register_service("user_service", instance2);
    registry->register_service("user_service", instance3);
    
    // 验证所有实例都健康
    auto instances_all = registry->get_instances("user_service");
    ASSERT_EQ(instances_all.size(), 3);
    
    // 将第二个实例标记为不健康
    registry->update_health_status("user_service", instance2, false);
    
    // 查询服务，应该只返回 2 个健康实例
    auto instances_partial = registry->get_instances("user_service");
    ASSERT_EQ(instances_partial.size(), 2);
    
    // 验证返回的是正确的实例
    std::vector<std::string> hosts;
    for (const auto& inst : instances_partial) {
        hosts.push_back(inst.host);
    }
    
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), "192.168.1.1") != hosts.end());
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), "192.168.1.3") != hosts.end());
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), "192.168.1.2") == hosts.end());
}

/**
 * @brief 测试更新不存在实例的健康状态
 * 
 * 验证：更新不存在实例的健康状态不应导致错误。
 */
TEST_F(ServiceRegistryTest, UpdateHealthStatusNonExistentInstance) {
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    
    // 更新不存在实例的健康状态（不应该崩溃）
    ASSERT_NO_THROW({
        registry->update_health_status("user_service", instance, false);
    });
}

/**
 * @brief 测试更新不存在服务的健康状态
 * 
 * 验证：更新不存在服务的健康状态不应导致错误。
 */
TEST_F(ServiceRegistryTest, UpdateHealthStatusNonExistentService) {
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    
    // 更新不存在服务的健康状态（不应该崩溃）
    ASSERT_NO_THROW({
        registry->update_health_status("non_existent_service", instance, false);
    });
}

// ============================================================================
// 服务变更通知测试
// ============================================================================

/**
 * @brief 测试服务变更通知 - 注册时触发
 * 
 * 验证：注册新实例时应触发变更通知。
 */
TEST_F(ServiceRegistryTest, SubscribeNotificationOnRegister) {
    std::atomic<int> notification_count{0};
    std::vector<ServiceInstance> notified_instances;
    
    // 订阅服务变更
    registry->subscribe("user_service", [&](const auto& instances) {
        notification_count++;
        notified_instances = instances;
    });
    
    // 注册服务实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance);
    
    // 验证收到通知
    EXPECT_EQ(notification_count, 1);
    ASSERT_EQ(notified_instances.size(), 1);
    EXPECT_EQ(notified_instances[0].host, "127.0.0.1");
    EXPECT_EQ(notified_instances[0].port, 8080);
}

/**
 * @brief 测试服务变更通知 - 注销时触发
 * 
 * 验证：注销实例时应触发变更通知。
 */
TEST_F(ServiceRegistryTest, SubscribeNotificationOnUnregister) {
    // 先注册一个实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance);
    
    std::atomic<int> notification_count{0};
    std::vector<ServiceInstance> notified_instances;
    
    // 订阅服务变更
    registry->subscribe("user_service", [&](const auto& instances) {
        notification_count++;
        notified_instances = instances;
    });
    
    // 注销服务实例
    registry->unregister_service("user_service", instance);
    
    // 验证收到通知
    EXPECT_EQ(notification_count, 1);
    EXPECT_TRUE(notified_instances.empty());
}

/**
 * @brief 测试服务变更通知 - 健康状态变化时触发
 * 
 * 验证：健康状态变化时应触发变更通知。
 */
TEST_F(ServiceRegistryTest, SubscribeNotificationOnHealthChange) {
    // 注册服务实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance);
    
    std::atomic<int> notification_count{0};
    
    // 订阅服务变更
    registry->subscribe("user_service", [&](const auto& instances) {
        notification_count++;
    });
    
    // 将实例标记为不健康（应触发通知）
    registry->update_health_status("user_service", instance, false);
    EXPECT_EQ(notification_count, 1);
    
    // 将实例标记为健康（应触发通知）
    registry->update_health_status("user_service", instance, true);
    EXPECT_EQ(notification_count, 2);
    
    // 重复标记为健康（不应触发通知，因为状态没有变化）
    registry->update_health_status("user_service", instance, true);
    EXPECT_EQ(notification_count, 2);
}

/**
 * @brief 测试多个订阅者
 * 
 * 验证：可以为同一个服务注册多个订阅者。
 */
TEST_F(ServiceRegistryTest, MultipleSubscribers) {
    std::atomic<int> notification_count1{0};
    std::atomic<int> notification_count2{0};
    std::atomic<int> notification_count3{0};
    
    // 注册多个订阅者
    registry->subscribe("user_service", [&](const auto&) {
        notification_count1++;
    });
    
    registry->subscribe("user_service", [&](const auto&) {
        notification_count2++;
    });
    
    registry->subscribe("user_service", [&](const auto&) {
        notification_count3++;
    });
    
    // 注册服务实例
    ServiceInstance instance{"127.0.0.1", 8080, 100};
    registry->register_service("user_service", instance);
    
    // 验证所有订阅者都收到通知
    EXPECT_EQ(notification_count1, 1);
    EXPECT_EQ(notification_count2, 1);
    EXPECT_EQ(notification_count3, 1);
}

// ============================================================================
// ServiceInstance 测试
// ============================================================================

/**
 * @brief 测试 ServiceInstance 相等比较
 * 
 * 验证：ServiceInstance 的相等比较基于 host 和 port。
 */
TEST(ServiceInstanceTest, Equality) {
    ServiceInstance instance1{"127.0.0.1", 8080, 100};
    ServiceInstance instance2{"127.0.0.1", 8080, 200};  // 不同的 weight
    ServiceInstance instance3{"127.0.0.1", 8081, 100};  // 不同的 port
    ServiceInstance instance4{"192.168.1.1", 8080, 100};  // 不同的 host
    
    // 相同的 host 和 port，即使 weight 不同也相等
    EXPECT_TRUE(instance1 == instance2);
    EXPECT_FALSE(instance1 != instance2);
    
    // 不同的 port
    EXPECT_FALSE(instance1 == instance3);
    EXPECT_TRUE(instance1 != instance3);
    
    // 不同的 host
    EXPECT_FALSE(instance1 == instance4);
    EXPECT_TRUE(instance1 != instance4);
}

/**
 * @brief 测试 ServiceInstance 默认权重
 * 
 * 验证：ServiceInstance 的默认权重为 100。
 */
TEST(ServiceInstanceTest, DefaultWeight) {
    ServiceInstance instance{"127.0.0.1", 8080};
    EXPECT_EQ(instance.weight, 100);
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
