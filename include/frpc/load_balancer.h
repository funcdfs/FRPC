/**
 * @file load_balancer.h
 * @brief 负载均衡器接口和实现
 * 
 * 提供多种负载均衡策略，用于从可用服务实例中选择一个实例处理请求。
 * 支持轮询、随机、最少连接和加权轮询等策略。
 * 
 * 主要功能：
 * - 轮询负载均衡（RoundRobinLoadBalancer）：按顺序依次选择实例
 * - 随机负载均衡（RandomLoadBalancer）：随机选择实例
 * - 最少连接负载均衡（LeastConnectionLoadBalancer）：选择连接数最少的实例
 * - 加权轮询负载均衡（WeightedRoundRobinLoadBalancer）：根据权重按比例选择实例
 * 
 * @note 所有负载均衡器都是线程安全的
 * @note 负载均衡器只从健康状态的服务实例中选择
 */

#ifndef FRPC_LOAD_BALANCER_H
#define FRPC_LOAD_BALANCER_H

#include "service_registry.h"
#include "exceptions.h"
#include <vector>
#include <atomic>
#include <random>
#include <mutex>

namespace frpc {

// Forward declaration
class ConnectionPool;

/**
 * @brief 负载均衡器接口
 * 
 * 定义负载均衡器的基本接口，所有具体的负载均衡策略都继承此接口。
 * 
 * 负载均衡器的职责是从可用的服务实例列表中选择一个实例处理请求。
 * 不同的策略有不同的选择算法，以满足不同的业务需求。
 * 
 * @note 所有实现类都应该是线程安全的
 * @note 如果没有可用实例，应该抛出 NoInstanceAvailableException
 */
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;
    
    /**
     * @brief 选择服务实例
     * 
     * 从可用的服务实例列表中选择一个实例处理请求。
     * 具体的选择策略由子类实现。
     * 
     * @param instances 可用的服务实例列表（应该只包含健康的实例）
     * @return 选中的服务实例
     * @throws NoInstanceAvailableException 如果没有可用实例
     * 
     * @note 线程安全：此方法应该是线程安全的
     * @note 输入的 instances 列表应该只包含健康的实例
     * @note 如果 instances 为空，应该抛出 NoInstanceAvailableException
     * 
     * @example
     * @code
     * std::vector<ServiceInstance> instances = registry.get_instances("user_service");
     * auto selected = load_balancer->select(instances);
     * auto conn = co_await pool.get_connection(selected);
     * @endcode
     */
    virtual ServiceInstance select(const std::vector<ServiceInstance>& instances) = 0;
};

/**
 * @brief 轮询负载均衡策略
 * 
 * 按顺序依次选择服务实例，形成循环。这是最简单和最常用的负载均衡策略。
 * 
 * **特点：**
 * - 简单高效，无需复杂计算
 * - 请求均匀分布到所有实例
 * - 不考虑实例的实际负载情况
 * 
 * **适用场景：**
 * - 所有服务实例性能相近
 * - 请求处理时间相对均匀
 * - 需要简单可靠的负载均衡
 * 
 * **工作原理：**
 * 维护一个原子计数器，每次选择时递增计数器并对实例数量取模，
 * 得到下一个要选择的实例索引。
 * 
 * @note 线程安全：使用原子操作保证线程安全
 * @note 性能：O(1) 时间复杂度，无锁设计
 * 
 * @example
 * @code
 * RoundRobinLoadBalancer lb;
 * 
 * // 假设有 3 个实例：A, B, C
 * // 连续调用 select() 会依次返回：A, B, C, A, B, C, ...
 * 
 * std::vector<ServiceInstance> instances = {
 *     {"192.168.1.1", 8080},
 *     {"192.168.1.2", 8080},
 *     {"192.168.1.3", 8080}
 * };
 * 
 * auto inst1 = lb.select(instances);  // 返回 192.168.1.1
 * auto inst2 = lb.select(instances);  // 返回 192.168.1.2
 * auto inst3 = lb.select(instances);  // 返回 192.168.1.3
 * auto inst4 = lb.select(instances);  // 返回 192.168.1.1（循环）
 * @endcode
 */
class RoundRobinLoadBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     */
    RoundRobinLoadBalancer() = default;
    
    /**
     * @brief 选择服务实例（轮询策略）
     * 
     * 按顺序依次选择实例，形成循环。
     * 
     * @param instances 可用的服务实例列表
     * @return 选中的服务实例
     * @throws NoInstanceAvailableException 如果没有可用实例
     */
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;

private:
    std::atomic<size_t> index_{0};  ///< 当前索引（原子操作保证线程安全）
};

/**
 * @brief 随机负载均衡策略
 * 
 * 随机选择服务实例。在大量请求的情况下，请求会均匀分布到所有实例。
 * 
 * **特点：**
 * - 实现简单，无状态
 * - 统计意义上的均匀分布
 * - 避免了轮询策略的顺序性
 * 
 * **适用场景：**
 * - 所有服务实例性能相近
 * - 不需要严格的顺序性
 * - 希望避免"惊群效应"
 * 
 * **工作原理：**
 * 使用随机数生成器从实例列表中随机选择一个实例。
 * 使用 std::mt19937 作为随机数生成器，提供良好的随机性。
 * 
 * @note 线程安全：使用互斥锁保护随机数生成器
 * @note 性能：O(1) 时间复杂度
 * @note 随机性：使用 Mersenne Twister 算法，随机性良好
 * 
 * @example
 * @code
 * RandomLoadBalancer lb;
 * 
 * // 假设有 3 个实例：A, B, C
 * // 连续调用 select() 会随机返回，例如：B, A, C, B, B, A, ...
 * 
 * std::vector<ServiceInstance> instances = {
 *     {"192.168.1.1", 8080},
 *     {"192.168.1.2", 8080},
 *     {"192.168.1.3", 8080}
 * };
 * 
 * // 在大量请求中，每个实例被选中的次数大致相等
 * std::unordered_map<std::string, int> counts;
 * for (int i = 0; i < 10000; ++i) {
 *     auto inst = lb.select(instances);
 *     counts[inst.host]++;
 * }
 * // counts 中每个实例的计数应该接近 10000/3 = 3333
 * @endcode
 */
class RandomLoadBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     * 
     * 使用随机设备初始化随机数生成器。
     */
    RandomLoadBalancer();
    
    /**
     * @brief 选择服务实例（随机策略）
     * 
     * 随机选择一个实例。
     * 
     * @param instances 可用的服务实例列表
     * @return 选中的服务实例
     * @throws NoInstanceAvailableException 如果没有可用实例
     */
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;

private:
    std::mt19937 rng_;      ///< 随机数生成器（Mersenne Twister）
    std::mutex mutex_;      ///< 保护随机数生成器的互斥锁
};

/**
 * @brief 最少连接负载均衡策略
 * 
 * 选择当前活跃连接数最少的服务实例。这种策略考虑了实例的实际负载情况。
 * 
 * **特点：**
 * - 考虑实例的实际负载
 * - 自动将请求分配到负载较轻的实例
 * - 适合处理时间差异较大的请求
 * 
 * **适用场景：**
 * - 请求处理时间差异较大
 * - 服务实例性能不均衡
 * - 需要根据实际负载动态调整
 * 
 * **工作原理：**
 * 遍历所有实例，查询每个实例的活跃连接数（通过 ConnectionPool），
 * 选择连接数最少的实例。
 * 
 * @note 线程安全：依赖 ConnectionPool 的线程安全性
 * @note 性能：O(n) 时间复杂度，n 为实例数量
 * @note 依赖：需要 ConnectionPool 提供连接数查询接口
 * 
 * @example
 * @code
 * ConnectionPool pool(config, &engine);
 * LeastConnectionLoadBalancer lb(&pool);
 * 
 * // 假设有 3 个实例：A(5 连接), B(2 连接), C(8 连接)
 * // select() 会返回 B（连接数最少）
 * 
 * std::vector<ServiceInstance> instances = {
 *     {"192.168.1.1", 8080},  // 5 个活跃连接
 *     {"192.168.1.2", 8080},  // 2 个活跃连接
 *     {"192.168.1.3", 8080}   // 8 个活跃连接
 * };
 * 
 * auto inst = lb.select(instances);  // 返回 192.168.1.2
 * @endcode
 */
class LeastConnectionLoadBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     * 
     * @param pool 连接池指针（用于查询连接数）
     * 
     * @note pool 指针必须在 LoadBalancer 生命周期内保持有效
     */
    explicit LeastConnectionLoadBalancer(ConnectionPool* pool);
    
    /**
     * @brief 选择服务实例（最少连接策略）
     * 
     * 选择当前活跃连接数最少的实例。
     * 
     * @param instances 可用的服务实例列表
     * @return 选中的服务实例
     * @throws NoInstanceAvailableException 如果没有可用实例
     */
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;

private:
    ConnectionPool* pool_;  ///< 连接池指针
};

/**
 * @brief 加权轮询负载均衡策略
 * 
 * 根据实例权重按比例选择服务实例。权重越大的实例被选中的概率越高。
 * 
 * **特点：**
 * - 考虑实例的性能差异
 * - 按权重比例分配请求
 * - 平滑的权重分配（避免突发）
 * 
 * **适用场景：**
 * - 服务实例性能不均衡
 * - 需要按比例分配流量
 * - 灰度发布（新版本设置较低权重）
 * 
 * **工作原理：**
 * 使用平滑加权轮询算法（Smooth Weighted Round-Robin）：
 * 1. 每个实例维护一个当前权重（初始为 0）
 * 2. 每次选择时，所有实例的当前权重加上其配置权重
 * 3. 选择当前权重最大的实例
 * 4. 将选中实例的当前权重减去所有实例的权重总和
 * 
 * 这种算法保证了权重分配的平滑性，避免了连续选择高权重实例的情况。
 * 
 * @note 线程安全：使用互斥锁保护内部状态
 * @note 性能：O(n) 时间复杂度，n 为实例数量
 * @note 权重：实例权重必须大于 0
 * 
 * @example
 * @code
 * WeightedRoundRobinLoadBalancer lb;
 * 
 * // 假设有 3 个实例：A(权重 5), B(权重 1), C(权重 1)
 * // 在 7 次选择中，A 会被选中 5 次，B 和 C 各被选中 1 次
 * // 选择顺序可能是：A, A, B, A, C, A, A（平滑分布）
 * 
 * std::vector<ServiceInstance> instances = {
 *     {"192.168.1.1", 8080, 5},  // 权重 5
 *     {"192.168.1.2", 8080, 1},  // 权重 1
 *     {"192.168.1.3", 8080, 1}   // 权重 1
 * };
 * 
 * // 统计 7 次选择
 * std::unordered_map<std::string, int> counts;
 * for (int i = 0; i < 7; ++i) {
 *     auto inst = lb.select(instances);
 *     counts[inst.host]++;
 * }
 * // counts["192.168.1.1"] == 5
 * // counts["192.168.1.2"] == 1
 * // counts["192.168.1.3"] == 1
 * @endcode
 */
class WeightedRoundRobinLoadBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     */
    WeightedRoundRobinLoadBalancer() = default;
    
    /**
     * @brief 选择服务实例（加权轮询策略）
     * 
     * 根据实例权重按比例选择实例，使用平滑加权轮询算法。
     * 
     * @param instances 可用的服务实例列表
     * @return 选中的服务实例
     * @throws NoInstanceAvailableException 如果没有可用实例
     */
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;

private:
    /**
     * @brief 实例权重状态
     * 
     * 维护每个实例的当前权重，用于平滑加权轮询算法。
     */
    struct WeightedInstance {
        ServiceInstance instance;   ///< 服务实例
        int current_weight;         ///< 当前权重（动态变化）
        int effective_weight;       ///< 有效权重（配置权重）
    };
    
    std::vector<WeightedInstance> weighted_instances_;  ///< 实例权重状态列表
    std::mutex mutex_;                                  ///< 保护内部状态的互斥锁
};

} // namespace frpc

#endif // FRPC_LOAD_BALANCER_H
