/**
 * @file health_checker.h
 * @brief 健康检测器
 * 
 * 定期向服务实例发送健康检测请求，根据响应情况更新实例的健康状态。
 * 健康检测器是服务治理的重要组件，确保只有健康的服务实例参与负载均衡。
 * 
 * 主要功能：
 * - 定期发送健康检测请求
 * - 跟踪连续失败次数
 * - 更新服务注册中心的实例健康状态
 * - 支持配置检测间隔、超时和失败阈值
 * - 线程安全的并发访问
 * 
 * 健康检测机制：
 * 1. 按配置的间隔时间定期向每个目标实例发送检测请求
 * 2. 如果实例响应，标记为健康状态，重置连续失败计数
 * 3. 如果实例未响应或超时，增加连续失败计数
 * 4. 当连续失败次数达到阈值时，标记为不健康状态并通知服务注册中心
 * 5. 不健康的实例继续检测，如果恢复响应则重新标记为健康状态
 * 
 * @note 健康检测使用协程实现，避免阻塞主线程
 * @note 所有公共方法都是线程安全的
 */

#ifndef FRPC_HEALTH_CHECKER_H
#define FRPC_HEALTH_CHECKER_H

#include "types.h"
#include "service_registry.h"
#include "coroutine.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>

namespace frpc {

/**
 * @brief 健康检测配置
 * 
 * 定义健康检测的各项参数，包括检测间隔、超时时间和失败阈值。
 * 
 * @example 创建健康检测配置
 * @code
 * HealthCheckConfig config{
 *     .interval = std::chrono::seconds(10),
 *     .timeout = std::chrono::seconds(3),
 *     .failure_threshold = 3,
 *     .success_threshold = 2
 * };
 * @endcode
 */
struct HealthCheckConfig {
    /**
     * @brief 检测间隔
     * 
     * 两次健康检测之间的时间间隔。
     * 间隔越短，能更快发现故障，但会增加网络开销。
     * 
     * 默认值：10 秒
     * 推荐范围：5-30 秒
     */
    std::chrono::seconds interval{10};
    
    /**
     * @brief 检测超时
     * 
     * 单次健康检测请求的超时时间。
     * 如果在超时时间内未收到响应，视为检测失败。
     * 
     * 默认值：3 秒
     * 推荐范围：1-5 秒
     * 
     * @note 超时时间应该小于检测间隔
     */
    std::chrono::seconds timeout{3};
    
    /**
     * @brief 失败阈值
     * 
     * 连续失败多少次后标记实例为不健康状态。
     * 阈值越大，容错性越好，但发现故障的时间越长。
     * 
     * 默认值：3 次
     * 推荐范围：2-5 次
     */
    int failure_threshold = 3;
    
    /**
     * @brief 成功阈值
     * 
     * 不健康的实例连续成功多少次后标记为健康状态。
     * 阈值越大，恢复越保守，但能避免频繁的状态切换。
     * 
     * 默认值：2 次
     * 推荐范围：1-3 次
     */
    int success_threshold = 2;
};

/**
 * @brief 健康检测器
 * 
 * 定期向服务实例发送健康检测请求，根据响应情况更新实例的健康状态。
 * 
 * 工作流程：
 * 1. 启动后创建检测循环协程
 * 2. 按配置的间隔时间定期检测所有目标实例
 * 3. 对每个实例发送检测请求（简单的 TCP 连接测试）
 * 4. 根据响应情况更新连续失败/成功计数
 * 5. 当计数达到阈值时，更新服务注册中心的健康状态
 * 
 * 线程安全：
 * - 使用 mutex 保护目标列表的并发访问
 * - 使用 atomic 标记运行状态
 * 
 * @example 基本使用
 * @code
 * HealthCheckConfig config{
 *     .interval = std::chrono::seconds(10),
 *     .timeout = std::chrono::seconds(3),
 *     .failure_threshold = 3
 * };
 * 
 * ServiceRegistry registry;
 * HealthChecker checker(config, &registry);
 * 
 * // 添加检测目标
 * ServiceInstance instance{"127.0.0.1", 8080, 100};
 * checker.add_target("user_service", instance);
 * 
 * // 启动健康检测
 * checker.start();
 * 
 * // ... 运行一段时间 ...
 * 
 * // 停止健康检测
 * checker.stop();
 * @endcode
 * 
 * @note 线程安全：所有公共方法都是线程安全的
 */
class HealthChecker {
public:
    /**
     * @brief 构造函数
     * 
     * 创建健康检测器实例，但不立即启动检测。
     * 需要调用 start() 方法启动检测循环。
     * 
     * @param config 健康检测配置
     * @param registry 服务注册中心指针（用于更新实例健康状态）
     * 
     * @note registry 指针必须在 HealthChecker 生命周期内保持有效
     * @note 不会获取 registry 的所有权
     * 
     * @example
     * @code
     * HealthCheckConfig config;
     * ServiceRegistry registry;
     * HealthChecker checker(config, &registry);
     * @endcode
     */
    HealthChecker(const HealthCheckConfig& config, ServiceRegistry* registry);
    
    /**
     * @brief 析构函数
     * 
     * 自动停止健康检测并清理资源。
     */
    ~HealthChecker();
    
    // 禁止拷贝和移动
    HealthChecker(const HealthChecker&) = delete;
    HealthChecker& operator=(const HealthChecker&) = delete;
    HealthChecker(HealthChecker&&) = delete;
    HealthChecker& operator=(HealthChecker&&) = delete;
    
    /**
     * @brief 启动健康检测
     * 
     * 启动检测循环协程，开始定期检测所有目标实例。
     * 如果已经启动，此方法不会产生任何效果。
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 启动后会立即进行第一次检测，然后按间隔定期检测
     * 
     * @example
     * @code
     * HealthChecker checker(config, &registry);
     * checker.add_target("user_service", instance);
     * checker.start();  // 开始检测
     * @endcode
     */
    void start();
    
    /**
     * @brief 停止健康检测
     * 
     * 停止检测循环协程，不再进行健康检测。
     * 如果已经停止，此方法不会产生任何效果。
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 停止后可以再次调用 start() 重新启动
     * 
     * @example
     * @code
     * checker.stop();  // 停止检测
     * @endcode
     */
    void stop();
    
    /**
     * @brief 添加检测目标
     * 
     * 将服务实例添加到健康检测目标列表。
     * 如果实例已存在（相同的 service_name、host 和 port），则不会重复添加。
     * 
     * @param service_name 服务名称
     * @param instance 服务实例
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 可以在检测运行时动态添加目标
     * @note 新添加的目标会在下一个检测周期开始检测
     * 
     * @example
     * @code
     * HealthChecker checker(config, &registry);
     * ServiceInstance instance{"127.0.0.1", 8080, 100};
     * checker.add_target("user_service", instance);
     * @endcode
     */
    void add_target(const std::string& service_name, 
                   const ServiceInstance& instance);
    
    /**
     * @brief 移除检测目标
     * 
     * 从健康检测目标列表中移除服务实例。
     * 如果实例不存在，此方法不会产生任何效果。
     * 
     * @param service_name 服务名称
     * @param instance 服务实例
     * 
     * @note 线程安全：此方法是线程安全的
     * @note 可以在检测运行时动态移除目标
     * 
     * @example
     * @code
     * ServiceInstance instance{"127.0.0.1", 8080, 100};
     * checker.remove_target("user_service", instance);
     * @endcode
     */
    void remove_target(const std::string& service_name,
                      const ServiceInstance& instance);

private:
    /**
     * @brief 检测目标信息（内部使用）
     * 
     * 扩展服务实例信息，添加健康检测相关的状态。
     */
    struct TargetInfo {
        std::string service_name;      ///< 服务名称
        ServiceInstance instance;      ///< 服务实例
        int consecutive_failures = 0;  ///< 连续失败次数
        int consecutive_successes = 0; ///< 连续成功次数（用于恢复）
        bool currently_healthy = true; ///< 当前健康状态
        
        /**
         * @brief 比较运算符
         * 
         * 两个目标相等当且仅当服务名称、主机地址和端口号都相同。
         */
        bool operator==(const TargetInfo& other) const {
            return service_name == other.service_name &&
                   instance.host == other.instance.host &&
                   instance.port == other.instance.port;
        }
    };
    
    /**
     * @brief 检测循环（协程）
     * 
     * 主检测循环，定期检测所有目标实例。
     * 此协程在 start() 时创建，在 stop() 时销毁。
     * 
     * 工作流程：
     * 1. 等待检测间隔时间
     * 2. 遍历所有目标实例
     * 3. 对每个实例调用 check_instance()
     * 4. 根据检测结果更新状态
     * 5. 重复步骤 1-4
     * 
     * @return RpcTask<void>
     */
    RpcTask<void> check_loop();
    
    /**
     * @brief 检测单个实例（协程）
     * 
     * 向指定实例发送健康检测请求，判断实例是否健康。
     * 
     * 检测方法：
     * 1. 尝试建立 TCP 连接到实例的地址和端口
     * 2. 如果连接成功，立即关闭连接，返回 true
     * 3. 如果连接失败或超时，返回 false
     * 
     * @param instance 服务实例
     * @return 检测是否成功（true 表示实例健康，false 表示不健康）
     * 
     * @note 使用简单的 TCP 连接测试作为健康检测
     * @note 实际应用中可以扩展为发送特定的健康检测协议消息
     */
    RpcTask<bool> check_instance(const ServiceInstance& instance);
    
    /**
     * @brief 健康检测配置
     */
    HealthCheckConfig config_;
    
    /**
     * @brief 服务注册中心指针
     * 
     * 用于更新实例的健康状态。
     * 不拥有所有权，调用者需要确保在 HealthChecker 生命周期内有效。
     */
    ServiceRegistry* registry_;
    
    /**
     * @brief 检测目标列表
     * 
     * 存储所有需要检测的服务实例及其状态。
     */
    std::vector<TargetInfo> targets_;
    
    /**
     * @brief 运行状态
     * 
     * 标记健康检测是否正在运行。
     * 使用 atomic 保证线程安全。
     */
    std::atomic<bool> running_{false};
    
    /**
     * @brief 互斥锁
     * 
     * 保护 targets_ 的并发访问。
     */
    std::mutex mutex_;
};

} // namespace frpc

#endif // FRPC_HEALTH_CHECKER_H
