/**
 * @file service_registry.h
 * @brief 服务注册中心
 * 
 * 维护服务名称到服务实例列表的映射，支持服务注册、注销、查询和变更通知。
 * 服务注册中心是服务发现机制的核心组件，允许服务提供者注册自己的实例信息，
 * 服务消费者查询可用的服务实例列表。
 * 
 * 主要功能：
 * - 服务实例注册和注销
 * - 查询健康的服务实例列表
 * - 订阅服务变更通知
 * - 更新实例健康状态
 * - 线程安全的并发访问
 * 
 * @note 所有公共方法都是线程安全的，使用读写锁保护共享数据
 */

#ifndef FRPC_SERVICE_REGISTRY_H
#define FRPC_SERVICE_REGISTRY_H

#include "types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <shared_mutex>
#include <chrono>

namespace frpc {

/**
 * @brief 服务实例信息
 * 
 * 描述一个服务实例的网络位置和权重信息。
 * 服务实例通过主机地址和端口唯一标识。
 * 
 * @example 创建服务实例
 * @code
 * ServiceInstance instance{
 *     .host = "192.168.1.100",
 *     .port = 8080,
 *     .weight = 100
 * };
 * @endcode
 */
struct ServiceInstance {
    /**
     * @brief 主机地址
     * 
     * 服务实例的 IP 地址或主机名。
     * 
     * @example "127.0.0.1", "192.168.1.100", "service.example.com"
     */
    std::string host;
    
    /**
     * @brief 端口号
     * 
     * 服务实例监听的端口号。
     */
    uint16_t port;
    
    /**
     * @brief 权重
     * 
     * 用于加权负载均衡的权重值。权重越大，被选中的概率越高。
     * 默认值为 100。
     * 
     * @note 权重必须大于 0
     */
    int weight = 100;
    
    /**
     * @brief 比较运算符
     * 
     * 两个服务实例相等当且仅当它们的主机地址和端口号都相同。
     * 权重不参与比较。
     * 
     * @param other 另一个服务实例
     * @return 如果两个实例相等返回 true，否则返回 false
     */
    bool operator==(const ServiceInstance& other) const {
        return host == other.host && port == other.port;
    }
    
    /**
     * @brief 不等于运算符
     */
    bool operator!=(const ServiceInstance& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 服务注册中心
 * 
 * 维护服务名称到服务实例列表的映射，支持动态服务发现。
 * 服务提供者在启动时注册自己的实例信息，服务消费者通过查询
 * 获取可用的服务实例列表。
 * 
 * 核心功能：
 * 1. 服务注册：服务提供者注册实例信息
 * 2. 服务注销：服务提供者注销实例信息
 * 3. 服务查询：服务消费者查询健康的实例列表
 * 4. 变更通知：实例列表变化时通知订阅者
 * 5. 健康管理：跟踪实例健康状态和心跳时间
 * 
 * 线程安全：
 * - 使用 shared_mutex（读写锁）保护共享数据
 * - 读操作（查询）可以并发执行
 * - 写操作（注册、注销、更新）互斥执行
 * 
 * @example 基本使用
 * @code
 * ServiceRegistry registry;
 * 
 * // 注册服务实例
 * ServiceInstance instance{"127.0.0.1", 8080, 100};
 * registry.register_service("user_service", instance);
 * 
 * // 查询服务实例
 * auto instances = registry.get_instances("user_service");
 * for (const auto& inst : instances) {
 *     std::cout << inst.host << ":" << inst.port << std::endl;
 * }
 * 
 * // 订阅服务变更
 * registry.subscribe("user_service", [](const auto& instances) {
 *     std::cout << "Service instances updated, count: " << instances.size() << std::endl;
 * });
 * 
 * // 注销服务实例
 * registry.unregister_service("user_service", instance);
 * @endcode
 * 
 * @note 线程安全：所有公共方法都是线程安全的
 */
class ServiceRegistry {
public:
    /**
     * @brief 服务变更回调函数类型
     * 
     * 当服务实例列表发生变化时，会调用此类型的回调函数。
     * 回调函数接收最新的健康服务实例列表作为参数。
     * 
     * @param instances 最新的健康服务实例列表
     */
    using ChangeCallback = std::function<void(const std::vector<ServiceInstance>&)>;
    
    /**
     * @brief 构造函数
     * 
     * 创建一个空的服务注册中心。
     */
    ServiceRegistry() = default;
    
    /**
     * @brief 析构函数
     */
    ~ServiceRegistry() = default;
    
    // 禁止拷贝和移动
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
    ServiceRegistry(ServiceRegistry&&) = delete;
    ServiceRegistry& operator=(ServiceRegistry&&) = delete;
    
    /**
     * @brief 注册服务实例
     * 
     * 将服务实例添加到指定服务名称的实例列表中。
     * 如果实例已存在（相同的 host 和 port），则更新其信息。
     * 
     * 注册成功后，会触发服务变更通知，通知所有订阅者。
     * 
     * @param service_name 服务名称
     * @param instance 服务实例信息
     * @return 注册是否成功
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 如果服务名称不存在，会自动创建
     * @note 新注册的实例默认标记为健康状态
     * 
     * @example
     * @code
     * ServiceRegistry registry;
     * ServiceInstance instance{"127.0.0.1", 8080, 100};
     * bool success = registry.register_service("user_service", instance);
     * if (success) {
     *     std::cout << "Service registered successfully" << std::endl;
     * }
     * @endcode
     */
    bool register_service(const std::string& service_name, 
                         const ServiceInstance& instance);
    
    /**
     * @brief 注销服务实例
     * 
     * 从指定服务名称的实例列表中移除服务实例。
     * 如果实例不存在，此操作不会产生任何效果。
     * 
     * 注销成功后，会触发服务变更通知，通知所有订阅者。
     * 
     * @param service_name 服务名称
     * @param instance 服务实例信息
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 如果服务名称不存在，此操作不会产生任何效果
     * 
     * @example
     * @code
     * ServiceRegistry registry;
     * ServiceInstance instance{"127.0.0.1", 8080, 100};
     * registry.unregister_service("user_service", instance);
     * @endcode
     */
    void unregister_service(const std::string& service_name,
                           const ServiceInstance& instance);
    
    /**
     * @brief 查询服务实例列表
     * 
     * 返回指定服务名称的所有健康服务实例列表。
     * 只返回健康状态的实例，不健康的实例会被过滤掉。
     * 
     * @param service_name 服务名称
     * @return 健康的服务实例列表（如果服务不存在，返回空列表）
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 返回的是实例的副本，调用者可以安全地使用
     * @note 只返回健康状态的实例
     * 
     * @example
     * @code
     * ServiceRegistry registry;
     * auto instances = registry.get_instances("user_service");
     * if (instances.empty()) {
     *     std::cout << "No healthy instances available" << std::endl;
     * } else {
     *     for (const auto& inst : instances) {
     *         std::cout << inst.host << ":" << inst.port << std::endl;
     *     }
     * }
     * @endcode
     */
    std::vector<ServiceInstance> get_instances(const std::string& service_name) const;
    
    /**
     * @brief 订阅服务变更通知
     * 
     * 注册一个回调函数，当指定服务的实例列表发生变化时，
     * 会调用此回调函数，传递最新的健康实例列表。
     * 
     * 变更包括：
     * - 新实例注册
     * - 实例注销
     * - 实例健康状态变化
     * 
     * @param service_name 服务名称
     * @param callback 变更回调函数
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 回调函数会在服务变更时同步调用，应避免在回调中执行耗时操作
     * @note 可以为同一个服务注册多个回调函数
     * 
     * @example
     * @code
     * ServiceRegistry registry;
     * registry.subscribe("user_service", [](const auto& instances) {
     *     std::cout << "Service updated, instance count: " << instances.size() << std::endl;
     *     for (const auto& inst : instances) {
     *         std::cout << "  - " << inst.host << ":" << inst.port << std::endl;
     *     }
     * });
     * @endcode
     */
    void subscribe(const std::string& service_name, 
                  ChangeCallback callback);
    
    /**
     * @brief 更新实例健康状态
     * 
     * 更新指定服务实例的健康状态和心跳时间。
     * 健康检测器会定期调用此方法更新实例状态。
     * 
     * 当实例的健康状态发生变化时（从健康变为不健康，或从不健康变为健康），
     * 会触发服务变更通知，通知所有订阅者。
     * 
     * @param service_name 服务名称
     * @param instance 服务实例
     * @param healthy 是否健康
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 如果实例不存在，此操作不会产生任何效果
     * @note 更新健康状态时会同时更新心跳时间
     * @note 只有健康状态发生变化时才会触发通知
     * 
     * @example
     * @code
     * ServiceRegistry registry;
     * ServiceInstance instance{"127.0.0.1", 8080, 100};
     * 
     * // 标记实例为不健康
     * registry.update_health_status("user_service", instance, false);
     * 
     * // 标记实例为健康
     * registry.update_health_status("user_service", instance, true);
     * @endcode
     */
    void update_health_status(const std::string& service_name,
                             const ServiceInstance& instance,
                             bool healthy);

private:
    /**
     * @brief 实例信息（内部使用）
     * 
     * 扩展 ServiceInstance，添加健康状态和心跳时间信息。
     */
    struct InstanceInfo {
        ServiceInstance instance;  ///< 服务实例
        bool healthy = true;       ///< 是否健康
        std::chrono::steady_clock::time_point last_heartbeat;  ///< 最后心跳时间
    };
    
    /**
     * @brief 通知订阅者
     * 
     * 当服务实例列表发生变化时，调用所有订阅者的回调函数。
     * 
     * @param service_name 服务名称
     * 
     * @note 此方法假设调用者已经持有写锁
     */
    void notify_subscribers(const std::string& service_name);
    
    /**
     * @brief 服务映射表
     * 
     * 从服务名称映射到实例信息列表。
     * 键：服务名称
     * 值：实例信息列表
     */
    std::unordered_map<std::string, std::vector<InstanceInfo>> services_;
    
    /**
     * @brief 订阅者映射表
     * 
     * 从服务名称映射到回调函数列表。
     * 键：服务名称
     * 值：回调函数列表
     */
    std::unordered_map<std::string, std::vector<ChangeCallback>> subscribers_;
    
    /**
     * @brief 读写锁
     * 
     * 保护 services_ 和 subscribers_ 的并发访问。
     * 使用 shared_mutex 允许多个读操作并发执行，
     * 但写操作与所有其他操作互斥。
     */
    mutable std::shared_mutex mutex_;
};

} // namespace frpc

#endif // FRPC_SERVICE_REGISTRY_H
