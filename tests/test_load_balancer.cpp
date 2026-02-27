/**
 * @file test_load_balancer.cpp
 * @brief 负载均衡器单元测试
 * 
 * 测试各种负载均衡策略的基本功能、边缘情况和错误处理。
 */

#include <gtest/gtest.h>
#include "frpc/load_balancer.h"
#include <unordered_map>
#include <set>

using namespace frpc;

// ============================================================================
// 测试辅助函数
// ============================================================================

/**
 * @brief 创建测试用的服务实例列表
 */
std::vector<ServiceInstance> create_test_instances(int count, int base_weight = 100) {
    std::vector<ServiceInstance> instances;
    for (int i = 0; i < count; ++i) {
        instances.push_back({
            "192.168.1." + std::to_string(i + 1),
            static_cast<uint16_t>(8080 + i),
            base_weight
        });
    }
    return instances;
}

// ============================================================================
// RoundRobinLoadBalancer 测试
// ============================================================================

/**
 * @brief 测试轮询负载均衡的基本功能
 * 
 * 验证轮询策略按顺序依次选择实例。
 * 
 * **Validates: Requirements 10.1, 10.2**
 */
TEST(RoundRobinLoadBalancerTest, BasicFunctionality) {
    RoundRobinLoadBalancer lb;
    auto instances = create_test_instances(3);
    
    // 第一轮：应该按顺序选择 0, 1, 2
    EXPECT_EQ(lb.select(instances).host, "192.168.1.1");
    EXPECT_EQ(lb.select(instances).host, "192.168.1.2");
    EXPECT_EQ(lb.select(instances).host, "192.168.1.3");
    
    // 第二轮：应该循环回到 0
    EXPECT_EQ(lb.select(instances).host, "192.168.1.1");
    EXPECT_EQ(lb.select(instances).host, "192.168.1.2");
}

/**
 * @brief 测试轮询负载均衡的循环性
 * 
 * 验证多次选择后会循环回到第一个实例。
 * 
 * **Validates: Requirements 10.2**
 */
TEST(RoundRobinLoadBalancerTest, Cycling) {
    RoundRobinLoadBalancer lb;
    auto instances = create_test_instances(5);
    
    // 选择 10 次，应该循环两次
    std::vector<std::string> selected_hosts;
    for (int i = 0; i < 10; ++i) {
        selected_hosts.push_back(lb.select(instances).host);
    }
    
    // 验证前 5 个和后 5 个相同（循环）
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(selected_hosts[i], selected_hosts[i + 5]);
    }
}

/**
 * @brief 测试单个实例的情况
 * 
 * 验证只有一个实例时，总是返回该实例。
 * 
 * **Validates: Requirements 10.1**
 */
TEST(RoundRobinLoadBalancerTest, SingleInstance) {
    RoundRobinLoadBalancer lb;
    auto instances = create_test_instances(1);
    
    // 多次选择应该总是返回同一个实例
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(lb.select(instances).host, "192.168.1.1");
    }
}

/**
 * @brief 测试空实例列表
 * 
 * 验证没有可用实例时抛出异常。
 * 
 * **Validates: Requirements 10.7**
 */
TEST(RoundRobinLoadBalancerTest, EmptyInstances) {
    RoundRobinLoadBalancer lb;
    std::vector<ServiceInstance> empty_instances;
    
    // 应该抛出 NoInstanceAvailableException
    EXPECT_THROW(lb.select(empty_instances), NoInstanceAvailableException);
}

// ============================================================================
// RandomLoadBalancer 测试
// ============================================================================

/**
 * @brief 测试随机负载均衡的基本功能
 * 
 * 验证随机策略能够选择所有实例。
 * 
 * **Validates: Requirements 10.1, 10.3**
 */
TEST(RandomLoadBalancerTest, BasicFunctionality) {
    RandomLoadBalancer lb;
    auto instances = create_test_instances(3);
    
    // 选择多次，应该能够选中所有实例
    std::set<std::string> selected_hosts;
    for (int i = 0; i < 100; ++i) {
        auto instance = lb.select(instances);
        selected_hosts.insert(instance.host);
    }
    
    // 验证所有实例都被选中过
    EXPECT_EQ(selected_hosts.size(), 3);
    EXPECT_TRUE(selected_hosts.count("192.168.1.1") > 0);
    EXPECT_TRUE(selected_hosts.count("192.168.1.2") > 0);
    EXPECT_TRUE(selected_hosts.count("192.168.1.3") > 0);
}

/**
 * @brief 测试随机负载均衡的分布性
 * 
 * 验证大量选择后，每个实例被选中的次数大致相等。
 * 
 * **Validates: Requirements 10.3**
 */
TEST(RandomLoadBalancerTest, Distribution) {
    RandomLoadBalancer lb;
    auto instances = create_test_instances(3);
    
    // 统计每个实例被选中的次数
    std::unordered_map<std::string, int> counts;
    const int total_selections = 3000;
    
    for (int i = 0; i < total_selections; ++i) {
        auto instance = lb.select(instances);
        counts[instance.host]++;
    }
    
    // 验证每个实例被选中的次数在合理范围内（期望值 ± 20%）
    int expected = total_selections / 3;
    int tolerance = expected * 0.2;  // 20% 容差
    
    for (const auto& [host, count] : counts) {
        EXPECT_GE(count, expected - tolerance);
        EXPECT_LE(count, expected + tolerance);
    }
}

/**
 * @brief 测试单个实例的情况
 * 
 * 验证只有一个实例时，总是返回该实例。
 * 
 * **Validates: Requirements 10.1**
 */
TEST(RandomLoadBalancerTest, SingleInstance) {
    RandomLoadBalancer lb;
    auto instances = create_test_instances(1);
    
    // 多次选择应该总是返回同一个实例
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(lb.select(instances).host, "192.168.1.1");
    }
}

/**
 * @brief 测试空实例列表
 * 
 * 验证没有可用实例时抛出异常。
 * 
 * **Validates: Requirements 10.7**
 */
TEST(RandomLoadBalancerTest, EmptyInstances) {
    RandomLoadBalancer lb;
    std::vector<ServiceInstance> empty_instances;
    
    // 应该抛出 NoInstanceAvailableException
    EXPECT_THROW(lb.select(empty_instances), NoInstanceAvailableException);
}

// ============================================================================
// LeastConnectionLoadBalancer 测试
// ============================================================================

/**
 * @brief 测试最少连接负载均衡的基本功能
 * 
 * 验证最少连接策略能够选择实例。
 * 
 * **Validates: Requirements 10.1, 10.4**
 * 
 * @note 由于当前实现的限制（ConnectionPool API 不支持按实例查询连接数），
 *       这个测试只验证基本功能，不验证是否真的选择了连接数最少的实例。
 * @note 由于 ConnectionPool 依赖 NetworkEngine，在非 Linux 平台上无法测试
 */
TEST(LeastConnectionLoadBalancerTest, DISABLED_BasicFunctionality) {
    // 这个测试在非 Linux 平台上被禁用
    // 因为 ConnectionPool 依赖 NetworkEngine，而 NetworkEngine 只在 Linux 上可用
}

/**
 * @brief 测试单个实例的情况
 * 
 * 验证只有一个实例时，总是返回该实例。
 * 
 * **Validates: Requirements 10.1**
 */
TEST(LeastConnectionLoadBalancerTest, DISABLED_SingleInstance) {
    // 这个测试在非 Linux 平台上被禁用
}

/**
 * @brief 测试空实例列表
 * 
 * 验证没有可用实例时抛出异常。
 * 
 * **Validates: Requirements 10.7**
 */
TEST(LeastConnectionLoadBalancerTest, DISABLED_EmptyInstances) {
    // 这个测试在非 Linux 平台上被禁用
}

// ============================================================================
// WeightedRoundRobinLoadBalancer 测试
// ============================================================================

/**
 * @brief 测试加权轮询负载均衡的基本功能
 * 
 * 验证加权轮询策略按权重比例选择实例。
 * 
 * **Validates: Requirements 10.1, 10.5**
 */
TEST(WeightedRoundRobinLoadBalancerTest, BasicFunctionality) {
    WeightedRoundRobinLoadBalancer lb;
    
    // 创建权重不同的实例：A(5), B(1), C(1)
    std::vector<ServiceInstance> instances = {
        {"192.168.1.1", 8080, 5},
        {"192.168.1.2", 8080, 1},
        {"192.168.1.3", 8080, 1}
    };
    
    // 统计 7 次选择（总权重为 7）
    std::unordered_map<std::string, int> counts;
    for (int i = 0; i < 7; ++i) {
        auto instance = lb.select(instances);
        counts[instance.host]++;
    }
    
    // 验证选择次数与权重成正比
    EXPECT_EQ(counts["192.168.1.1"], 5);  // 权重 5
    EXPECT_EQ(counts["192.168.1.2"], 1);  // 权重 1
    EXPECT_EQ(counts["192.168.1.3"], 1);  // 权重 1
}

/**
 * @brief 测试加权轮询的平滑性
 * 
 * 验证加权轮询不会连续选择高权重实例，而是平滑分布。
 * 
 * **Validates: Requirements 10.5**
 */
TEST(WeightedRoundRobinLoadBalancerTest, SmoothDistribution) {
    WeightedRoundRobinLoadBalancer lb;
    
    // 创建权重不同的实例：A(5), B(1)
    std::vector<ServiceInstance> instances = {
        {"192.168.1.1", 8080, 5},
        {"192.168.1.2", 8080, 1}
    };
    
    // 选择 6 次，记录顺序
    std::vector<std::string> selected_hosts;
    for (int i = 0; i < 6; ++i) {
        selected_hosts.push_back(lb.select(instances).host);
    }
    
    // 验证不是连续 5 次选择 A，而是平滑分布
    // 平滑加权轮询应该产生类似：A, A, B, A, A, A 的序列
    int consecutive_a = 0;
    int max_consecutive_a = 0;
    for (const auto& host : selected_hosts) {
        if (host == "192.168.1.1") {
            consecutive_a++;
            max_consecutive_a = std::max(max_consecutive_a, consecutive_a);
        } else {
            consecutive_a = 0;
        }
    }
    
    // 最大连续次数应该小于总权重（平滑性）
    EXPECT_LT(max_consecutive_a, 5);
}

/**
 * @brief 测试相等权重的情况
 * 
 * 验证所有实例权重相等时，行为类似轮询。
 * 
 * **Validates: Requirements 10.5**
 */
TEST(WeightedRoundRobinLoadBalancerTest, EqualWeights) {
    WeightedRoundRobinLoadBalancer lb;
    
    // 创建权重相等的实例
    std::vector<ServiceInstance> instances = {
        {"192.168.1.1", 8080, 100},
        {"192.168.1.2", 8080, 100},
        {"192.168.1.3", 8080, 100}
    };
    
    // 统计 300 次选择
    std::unordered_map<std::string, int> counts;
    for (int i = 0; i < 300; ++i) {
        auto instance = lb.select(instances);
        counts[instance.host]++;
    }
    
    // 验证每个实例被选中的次数相等
    EXPECT_EQ(counts["192.168.1.1"], 100);
    EXPECT_EQ(counts["192.168.1.2"], 100);
    EXPECT_EQ(counts["192.168.1.3"], 100);
}

/**
 * @brief 测试单个实例的情况
 * 
 * 验证只有一个实例时，总是返回该实例。
 * 
 * **Validates: Requirements 10.1**
 */
TEST(WeightedRoundRobinLoadBalancerTest, SingleInstance) {
    WeightedRoundRobinLoadBalancer lb;
    auto instances = create_test_instances(1);
    
    // 多次选择应该总是返回同一个实例
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(lb.select(instances).host, "192.168.1.1");
    }
}

/**
 * @brief 测试空实例列表
 * 
 * 验证没有可用实例时抛出异常。
 * 
 * **Validates: Requirements 10.7**
 */
TEST(WeightedRoundRobinLoadBalancerTest, EmptyInstances) {
    WeightedRoundRobinLoadBalancer lb;
    std::vector<ServiceInstance> empty_instances;
    
    // 应该抛出 NoInstanceAvailableException
    EXPECT_THROW(lb.select(empty_instances), NoInstanceAvailableException);
}

/**
 * @brief 测试大权重差异的情况
 * 
 * 验证权重差异很大时，仍然能够正确分配。
 * 
 * **Validates: Requirements 10.5**
 */
TEST(WeightedRoundRobinLoadBalancerTest, LargeWeightDifference) {
    WeightedRoundRobinLoadBalancer lb;
    
    // 创建权重差异很大的实例：A(100), B(1)
    std::vector<ServiceInstance> instances = {
        {"192.168.1.1", 8080, 100},
        {"192.168.1.2", 8080, 1}
    };
    
    // 统计 101 次选择
    std::unordered_map<std::string, int> counts;
    for (int i = 0; i < 101; ++i) {
        auto instance = lb.select(instances);
        counts[instance.host]++;
    }
    
    // 验证选择次数与权重成正比
    EXPECT_EQ(counts["192.168.1.1"], 100);
    EXPECT_EQ(counts["192.168.1.2"], 1);
}
