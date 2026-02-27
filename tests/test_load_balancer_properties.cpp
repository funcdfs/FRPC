/**
 * @file test_load_balancer_properties.cpp
 * @brief 负载均衡器属性测试
 * 
 * 使用 RapidCheck 进行基于属性的测试，验证负载均衡器的通用属性。
 */

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include "frpc/load_balancer.h"
#include <unordered_map>
#include <set>
#include <algorithm>

using namespace frpc;

// ============================================================================
// 测试辅助函数和生成器
// ============================================================================

/**
 * @brief 生成随机的服务实例
 */
namespace rc {
template<>
struct Arbitrary<ServiceInstance> {
    static Gen<ServiceInstance> arbitrary() {
        return gen::build<ServiceInstance>(
            gen::set(&ServiceInstance::host, 
                gen::element<std::string>("192.168.1.1", "192.168.1.2", "192.168.1.3", 
                                         "10.0.0.1", "10.0.0.2", "127.0.0.1")),
            gen::set(&ServiceInstance::port, 
                gen::inRange<uint16_t>(8000, 9000)),
            gen::set(&ServiceInstance::weight, 
                gen::inRange(1, 100))
        );
    }
};
}

/**
 * @brief 生成非空的服务实例列表
 */
rc::Gen<std::vector<ServiceInstance>> genNonEmptyInstances() {
    return rc::gen::nonEmpty(rc::gen::unique<std::vector<ServiceInstance>>(
        rc::gen::arbitrary<ServiceInstance>()
    ));
}

// ============================================================================
// Property 24: 负载均衡器只选择健康实例
// ============================================================================

/**
 * @brief Property 24: 负载均衡器只选择健康实例
 * 
 * 验证负载均衡器只从输入的实例列表中选择实例，不会选择不存在的实例。
 * 
 * **属性描述：**
 * For any 负载均衡请求，Load Balancer 应该只从健康状态的服务实例中选择，
 * 不应该选择不健康或已移除的实例。
 * 
 * **测试策略：**
 * 由于负载均衡器本身不维护健康状态（健康状态由 ServiceRegistry 维护），
 * 我们测试负载均衡器只从传入的实例列表中选择，不会选择列表外的实例。
 * 
 * **Validates: Requirements 10.1, 10.6**
 * 
 * Feature: frpc-framework, Property 24: 负载均衡器只选择健康实例
 */
RC_GTEST_PROP(LoadBalancerPropertyTest, OnlySelectsHealthyInstances, 
              (const std::vector<ServiceInstance>& instances)) {
    RC_PRE(!instances.empty());  // 前置条件：实例列表非空
    
    // 测试所有负载均衡策略
    std::vector<std::unique_ptr<LoadBalancer>> load_balancers;
    load_balancers.push_back(std::make_unique<RoundRobinLoadBalancer>());
    load_balancers.push_back(std::make_unique<RandomLoadBalancer>());
    load_balancers.push_back(std::make_unique<WeightedRoundRobinLoadBalancer>());
    
    for (auto& lb : load_balancers) {
        // 选择多次
        for (int i = 0; i < 100; ++i) {
            auto selected = lb->select(instances);
            
            // 验证选中的实例在输入列表中
            bool found = std::any_of(instances.begin(), instances.end(),
                [&selected](const ServiceInstance& inst) {
                    return inst.host == selected.host && inst.port == selected.port;
                });
            
            RC_ASSERT(found);
        }
    }
}

// ============================================================================
// Property 25: 轮询负载均衡顺序性
// ============================================================================

/**
 * @brief Property 25: 轮询负载均衡顺序性
 * 
 * 验证轮询负载均衡器按顺序依次选择实例，形成循环。
 * 
 * **属性描述：**
 * For any 使用轮询策略的负载均衡器和服务实例列表，连续的选择操作应该
 * 按照实例在列表中的顺序依次返回实例，形成循环。
 * 
 * **测试策略：**
 * 1. 选择 N 次（N = 实例数量），验证顺序与输入列表一致
 * 2. 再选择 N 次，验证循环回到第一个实例
 * 
 * **Validates: Requirements 10.2**
 * 
 * Feature: frpc-framework, Property 25: 轮询负载均衡顺序性
 */
RC_GTEST_PROP(LoadBalancerPropertyTest, RoundRobinOrder,
              (const std::vector<ServiceInstance>& instances)) {
    RC_PRE(!instances.empty());  // 前置条件：实例列表非空
    
    RoundRobinLoadBalancer lb;
    
    // 第一轮：选择 N 次
    std::vector<ServiceInstance> first_round;
    for (size_t i = 0; i < instances.size(); ++i) {
        first_round.push_back(lb.select(instances));
    }
    
    // 验证第一轮的顺序与输入列表一致
    for (size_t i = 0; i < instances.size(); ++i) {
        RC_ASSERT(first_round[i].host == instances[i].host);
        RC_ASSERT(first_round[i].port == instances[i].port);
    }
    
    // 第二轮：再选择 N 次
    std::vector<ServiceInstance> second_round;
    for (size_t i = 0; i < instances.size(); ++i) {
        second_round.push_back(lb.select(instances));
    }
    
    // 验证第二轮与第一轮相同（循环）
    for (size_t i = 0; i < instances.size(); ++i) {
        RC_ASSERT(second_round[i].host == first_round[i].host);
        RC_ASSERT(second_round[i].port == first_round[i].port);
    }
}

// ============================================================================
// Property 26: 随机负载均衡分布性
// ============================================================================

/**
 * @brief Property 26: 随机负载均衡分布性
 * 
 * 验证随机负载均衡器在大量选择中，每个实例被选中的次数大致相等。
 * 
 * **属性描述：**
 * For any 使用随机策略的负载均衡器和服务实例列表，在大量选择操作中，
 * 每个实例被选中的次数应该大致相等（统计意义上的均匀分布）。
 * 
 * **测试策略：**
 * 1. 选择 N * 100 次（N = 实例数量）
 * 2. 统计每个实例被选中的次数
 * 3. 验证每个实例的选中次数在期望值 ± 30% 范围内
 * 
 * **Validates: Requirements 10.3**
 * 
 * Feature: frpc-framework, Property 26: 随机负载均衡分布性
 */
RC_GTEST_PROP(LoadBalancerPropertyTest, RandomDistribution,
              (const std::vector<ServiceInstance>& instances)) {
    RC_PRE(instances.size() >= 2 && instances.size() <= 10);  // 限制实例数量
    
    RandomLoadBalancer lb;
    
    // 统计每个实例被选中的次数
    std::unordered_map<std::string, int> counts;
    const int total_selections = instances.size() * 100;
    
    for (int i = 0; i < total_selections; ++i) {
        auto selected = lb.select(instances);
        std::string key = selected.host + ":" + std::to_string(selected.port);
        counts[key]++;
    }
    
    // 验证每个实例被选中的次数在合理范围内
    int expected = total_selections / instances.size();
    int tolerance = expected * 0.3;  // 30% 容差
    
    for (const auto& instance : instances) {
        std::string key = instance.host + ":" + std::to_string(instance.port);
        int count = counts[key];
        
        RC_ASSERT(count >= expected - tolerance);
        RC_ASSERT(count <= expected + tolerance);
    }
}

// ============================================================================
// Property 27: 最少连接负载均衡
// ============================================================================

/**
 * @brief Property 27: 最少连接负载均衡
 * 
 * 验证最少连接负载均衡器选择连接数最少的实例。
 * 
 * **属性描述：**
 * For any 使用最少连接策略的负载均衡器，选择的实例应该是当前活跃连接数最少的实例。
 * 
 * **测试策略：**
 * 由于当前实现的限制（ConnectionPool API 不支持按实例查询连接数），
 * 这个测试暂时被禁用。需要扩展 ConnectionPool API 后才能实现。
 * 
 * **Validates: Requirements 10.4**
 * 
 * Feature: frpc-framework, Property 27: 最少连接负载均衡
 */
TEST(LoadBalancerPropertyTest, DISABLED_LeastConnectionProperty) {
    // 这个测试需要扩展 ConnectionPool API 才能实现
    // TODO: 实现 ConnectionPool::get_instance_stats(instance) 方法
    // 然后实现这个属性测试
}

// ============================================================================
// Property 28: 加权轮询负载均衡
// ============================================================================

/**
 * @brief Property 28: 加权轮询负载均衡
 * 
 * 验证加权轮询负载均衡器按权重比例选择实例。
 * 
 * **属性描述：**
 * For any 使用加权轮询策略的负载均衡器和带权重的服务实例列表，
 * 在大量选择操作中，每个实例被选中的次数应该与其权重成正比。
 * 
 * **测试策略：**
 * 1. 计算总权重
 * 2. 选择 total_weight * 10 次
 * 3. 统计每个实例被选中的次数
 * 4. 验证每个实例的选中次数与权重成正比（误差 ± 20%）
 * 
 * **Validates: Requirements 10.5**
 * 
 * Feature: frpc-framework, Property 28: 加权轮询负载均衡
 */
RC_GTEST_PROP(LoadBalancerPropertyTest, WeightedRoundRobinProportion,
              (const std::vector<ServiceInstance>& instances)) {
    RC_PRE(instances.size() >= 2 && instances.size() <= 5);  // 限制实例数量
    RC_PRE(std::all_of(instances.begin(), instances.end(),
        [](const ServiceInstance& inst) { return inst.weight > 0; }));  // 权重必须大于 0
    
    WeightedRoundRobinLoadBalancer lb;
    
    // 计算总权重
    int total_weight = 0;
    for (const auto& instance : instances) {
        total_weight += instance.weight;
    }
    
    // 统计每个实例被选中的次数
    std::unordered_map<std::string, int> counts;
    const int total_selections = total_weight * 10;
    
    for (int i = 0; i < total_selections; ++i) {
        auto selected = lb.select(instances);
        std::string key = selected.host + ":" + std::to_string(selected.port);
        counts[key]++;
    }
    
    // 验证每个实例被选中的次数与权重成正比
    for (const auto& instance : instances) {
        std::string key = instance.host + ":" + std::to_string(instance.port);
        int count = counts[key];
        
        // 期望值 = total_selections * (weight / total_weight)
        int expected = (total_selections * instance.weight) / total_weight;
        int tolerance = std::max(1, expected / 5);  // 20% 容差，最小为 1
        
        RC_ASSERT(count >= expected - tolerance);
        RC_ASSERT(count <= expected + tolerance);
    }
}

/**
 * @brief 加权轮询的平滑性属性
 * 
 * 验证加权轮询不会连续选择高权重实例，而是平滑分布。
 * 
 * **属性描述：**
 * 加权轮询应该使用平滑加权算法，避免连续选择高权重实例。
 * 
 * **测试策略：**
 * 1. 创建权重差异较大的实例列表
 * 2. 选择多次，记录顺序
 * 3. 验证最大连续选择次数小于实例权重
 * 
 * **Validates: Requirements 10.5**
 * 
 * Feature: frpc-framework, Property 28: 加权轮询负载均衡（平滑性）
 */
RC_GTEST_PROP(LoadBalancerPropertyTest, WeightedRoundRobinSmoothness,
              (int high_weight, int low_weight)) {
    RC_PRE(high_weight >= 3 && high_weight <= 10);
    RC_PRE(low_weight >= 1 && low_weight <= 2);
    RC_PRE(high_weight > low_weight);
    
    WeightedRoundRobinLoadBalancer lb;
    
    // 创建两个实例：一个高权重，一个低权重
    std::vector<ServiceInstance> instances = {
        {"192.168.1.1", 8080, high_weight},
        {"192.168.1.2", 8080, low_weight}
    };
    
    // 选择 (high_weight + low_weight) * 2 次
    int total_selections = (high_weight + low_weight) * 2;
    std::vector<std::string> selected_hosts;
    
    for (int i = 0; i < total_selections; ++i) {
        auto selected = lb.select(instances);
        selected_hosts.push_back(selected.host);
    }
    
    // 计算最大连续选择次数
    int consecutive_count = 1;
    int max_consecutive = 1;
    
    for (size_t i = 1; i < selected_hosts.size(); ++i) {
        if (selected_hosts[i] == selected_hosts[i - 1]) {
            consecutive_count++;
            max_consecutive = std::max(max_consecutive, consecutive_count);
        } else {
            consecutive_count = 1;
        }
    }
    
    // 验证最大连续次数小于权重（平滑性）
    // 平滑加权轮询应该避免连续选择高权重实例
    RC_ASSERT(max_consecutive < high_weight);
}
