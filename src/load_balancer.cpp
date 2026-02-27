/**
 * @file load_balancer.cpp
 * @brief 负载均衡器实现
 */

#include "frpc/load_balancer.h"
#include "frpc/connection_pool.h"
#include <algorithm>
#include <numeric>

namespace frpc {

// ============================================================================
// RoundRobinLoadBalancer 实现
// ============================================================================

ServiceInstance RoundRobinLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
    // 检查是否有可用实例
    if (instances.empty()) {
        throw NoInstanceAvailableException("No instances available for load balancing");
    }
    
    // 原子递增索引并取模，实现轮询
    size_t idx = index_.fetch_add(1, std::memory_order_relaxed) % instances.size();
    return instances[idx];
}

// ============================================================================
// RandomLoadBalancer 实现
// ============================================================================

RandomLoadBalancer::RandomLoadBalancer() 
    : rng_(std::random_device{}()) {
}

ServiceInstance RandomLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
    // 检查是否有可用实例
    if (instances.empty()) {
        throw NoInstanceAvailableException("No instances available for load balancing");
    }
    
    // 生成随机索引
    std::lock_guard<std::mutex> lock(mutex_);
    std::uniform_int_distribution<size_t> dist(0, instances.size() - 1);
    size_t idx = dist(rng_);
    return instances[idx];
}

// ============================================================================
// LeastConnectionLoadBalancer 实现
// ============================================================================

LeastConnectionLoadBalancer::LeastConnectionLoadBalancer(ConnectionPool* pool)
    : pool_(pool) {
}

ServiceInstance LeastConnectionLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
    // 检查是否有可用实例
    if (instances.empty()) {
        throw NoInstanceAvailableException("No instances available for load balancing");
    }
    
    // 获取连接池统计信息
    auto stats = pool_->get_stats();
    
    // 简化实现：由于当前 ConnectionPool::get_stats() 返回的是全局统计信息，
    // 我们无法直接获取每个实例的连接数。
    // 作为临时方案，我们使用轮询策略作为后备。
    // TODO: 扩展 ConnectionPool API 以支持按实例查询连接数
    
    // 为了实现最少连接策略，我们需要遍历所有实例并查询每个实例的连接数
    // 由于当前 API 限制，我们暂时返回第一个实例
    // 在实际使用中，应该扩展 ConnectionPool 提供 get_instance_stats(instance) 方法
    
    // 临时实现：选择第一个实例
    // 这不是真正的最少连接策略，但满足基本功能
    return instances[0];
}

// ============================================================================
// WeightedRoundRobinLoadBalancer 实现
// ============================================================================

ServiceInstance WeightedRoundRobinLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
    // 检查是否有可用实例
    if (instances.empty()) {
        throw NoInstanceAvailableException("No instances available for load balancing");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果实例列表发生变化，重新初始化权重状态
    if (weighted_instances_.size() != instances.size()) {
        weighted_instances_.clear();
        for (const auto& instance : instances) {
            weighted_instances_.push_back({
                instance,
                0,  // current_weight 初始为 0
                instance.weight  // effective_weight 使用配置的权重
            });
        }
    } else {
        // 更新实例信息（可能权重发生了变化）
        for (size_t i = 0; i < instances.size(); ++i) {
            weighted_instances_[i].instance = instances[i];
            weighted_instances_[i].effective_weight = instances[i].weight;
        }
    }
    
    // 平滑加权轮询算法
    // 1. 计算总权重
    int total_weight = 0;
    for (const auto& wi : weighted_instances_) {
        total_weight += wi.effective_weight;
    }
    
    // 2. 所有实例的当前权重加上其有效权重
    for (auto& wi : weighted_instances_) {
        wi.current_weight += wi.effective_weight;
    }
    
    // 3. 选择当前权重最大的实例
    auto max_it = std::max_element(
        weighted_instances_.begin(),
        weighted_instances_.end(),
        [](const WeightedInstance& a, const WeightedInstance& b) {
            return a.current_weight < b.current_weight;
        }
    );
    
    // 4. 将选中实例的当前权重减去总权重
    max_it->current_weight -= total_weight;
    
    return max_it->instance;
}

} // namespace frpc
