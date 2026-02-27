/**
 * @file connection_pool.h
 * @brief 连接池管理器和连接类定义
 * 
 * 提供高效的 TCP 连接管理，支持连接复用、自动清理和健康检测。
 * 连接池维护到每个服务实例的连接集合，优先返回空闲连接以降低握手开销。
 */

#pragma once

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "coroutine.h"
#include "network_engine.h"
#include "types.h"
#include "service_registry.h"

namespace frpc {

// Forward declarations
class NetworkEngine;
struct PoolConfig;

// Hash function for ServiceInstance (for unordered_map)
struct ServiceInstanceHash {
    size_t operator()(const ServiceInstance& instance) const {
        size_t h1 = std::hash<std::string>{}(instance.host);
        size_t h2 = std::hash<uint16_t>{}(instance.port);
        return h1 ^ (h2 << 1);
    }
};

/**
 * @brief 连接对象
 * 
 * 封装一个 TCP 连接，包括 socket 文件描述符和服务实例信息。
 * 提供协程友好的发送和接收方法，自动委托给 NetworkEngine 进行异步 I/O。
 * 
 * **核心功能：**
 * 
 * 1. **异步 I/O**：
 *    - send() 和 recv() 方法返回 RpcTask，支持 co_await 语法
 *    - 所有网络操作都是非阻塞的，不会阻塞协程调度器
 * 
 * 2. **连接状态管理**：
 *    - is_valid() 检查连接是否有效（未关闭、未出错）
 *    - 记录创建时间和最后使用时间，用于空闲超时检测
 * 
 * 3. **生命周期管理**：
 *    - 支持移动语义，禁止拷贝
 *    - 析构时自动关闭 socket（如果有效）
 * 
 * **使用场景：**
 * 
 * Connection 对象通常从 ConnectionPool 获取，使用完毕后归还：
 * 
 * @code
 * // 从连接池获取连接
 * auto conn = co_await pool.get_connection(instance);
 * 
 * // 发送请求
 * std::vector<uint8_t> request_data = serialize_request(request);
 * auto send_result = co_await conn.send(request_data);
 * 
 * // 接收响应
 * auto response_data = co_await conn.recv();
 * 
 * // 归还连接
 * pool.return_connection(std::move(conn));
 * @endcode
 * 
 * @note 线程安全：Connection 对象本身不是线程安全的，不应在多个线程间共享
 * @note 资源管理：使用 RAII 管理 socket 生命周期，析构时自动关闭
 * @note 性能：所有 I/O 操作都是异步的，不会阻塞事件循环
 */
class Connection {
public:
    /**
     * @brief 构造函数
     * 
     * 创建一个新的连接对象，封装 socket 文件描述符和服务实例信息。
     * 
     * @param fd socket 文件描述符（必须是有效的、已连接的 socket）
     * @param instance 服务实例信息
     * @param engine 网络引擎指针（用于异步 I/O 操作）
     * 
     * @note fd 必须是有效的 socket，且已经通过 connect() 连接到远程服务器
     * @note engine 指针必须在 Connection 生命周期内保持有效
     * 
     * @example
     * @code
     * int fd = co_await engine.async_connect("127.0.0.1", 8080);
     * ServiceInstance instance{"127.0.0.1", 8080};
     * Connection conn(fd, instance, &engine);
     * @endcode
     */
    Connection(int fd, const ServiceInstance& instance, NetworkEngine* engine);
    
    /**
     * @brief 析构函数 - 自动关闭连接
     * 
     * 如果连接仍然有效，析构时会自动关闭 socket 并注销网络事件。
     * 
     * @note RAII：确保资源被正确释放
     */
    ~Connection();
    
    // 禁止拷贝
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    
    // 允许移动
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;
    
    /**
     * @brief 发送数据（协程方法）
     * 
     * 异步发送数据到远程服务器。此方法返回一个 RpcTask，
     * 可以在协程中使用 co_await 等待发送完成。
     * 
     * @param data 要发送的数据
     * @return RpcTask<SendResult> 发送结果
     * 
     * **工作流程：**
     * 1. 调用 NetworkEngine::async_send() 发起异步发送
     * 2. 如果 socket 缓冲区满，协程会挂起
     * 3. 当 socket 可写时，NetworkEngine 恢复协程
     * 4. 返回发送结果（成功/失败、发送字节数）
     * 
     * **错误处理：**
     * - 如果发送失败，SendResult.success 为 false
     * - 调用者应该检查返回值并处理错误
     * - 发送失败后，连接应该被关闭并从连接池移除
     * 
     * @note 此方法会更新 last_used_at_ 时间戳
     * @note 如果连接已关闭，返回失败结果
     * 
     * @example
     * @code
     * std::vector<uint8_t> data = {1, 2, 3, 4, 5};
     * auto result = co_await conn.send(data);
     * if (result.success) {
     *     std::cout << "Sent " << result.bytes_sent << " bytes" << std::endl;
     * } else {
     *     std::cerr << "Send failed: " << result.error_code << std::endl;
     *     // 连接已损坏，应该关闭
     * }
     * @endcode
     */
    RpcTask<SendResult> send(std::span<const uint8_t> data);
    
    /**
     * @brief 接收数据（协程方法）
     * 
     * 异步接收数据从远程服务器。此方法返回一个 RpcTask，
     * 可以在协程中使用 co_await 等待接收完成。
     * 
     * @return RpcTask<std::vector<uint8_t>> 接收到的数据
     * 
     * **工作流程：**
     * 1. 调用 NetworkEngine::async_recv() 发起异步接收
     * 2. 如果没有数据可读，协程会挂起
     * 3. 当 socket 有数据可读时，NetworkEngine 恢复协程
     * 4. 返回接收到的数据
     * 
     * **返回值：**
     * - 成功：返回包含数据的 vector
     * - 连接关闭：返回空 vector
     * - 错误：抛出 NetworkException
     * 
     * **错误处理：**
     * - 如果接收失败，抛出 NetworkException
     * - 如果连接被远程关闭，返回空 vector
     * - 调用者应该检查返回值并处理错误
     * 
     * @throws NetworkException 如果接收失败
     * 
     * @note 此方法会更新 last_used_at_ 时间戳
     * @note 如果连接已关闭，返回空 vector
     * 
     * @example
     * @code
     * auto data = co_await conn.recv();
     * if (!data.empty()) {
     *     std::cout << "Received " << data.size() << " bytes" << std::endl;
     *     // 处理数据...
     * } else {
     *     std::cout << "Connection closed by remote" << std::endl;
     * }
     * @endcode
     */
    RpcTask<std::vector<uint8_t>> recv();
    
    /**
     * @brief 检查连接是否有效
     * 
     * 检查连接是否仍然有效（未关闭、未出错）。
     * 
     * @return true 如果连接有效
     * @return false 如果连接已关闭或出错
     * 
     * **有效性判断：**
     * - fd_ > 0：socket 文件描述符有效
     * - 未来可以扩展：检查 socket 错误状态、TCP keepalive 等
     * 
     * @note 此方法只检查本地状态，不进行网络 I/O
     * @note 即使此方法返回 true，后续的 send/recv 操作仍可能失败
     * 
     * @example
     * @code
     * if (conn.is_valid()) {
     *     // 连接有效，可以使用
     *     auto result = co_await conn.send(data);
     * } else {
     *     // 连接已关闭，需要重新建立
     *     conn = co_await pool.get_connection(instance);
     * }
     * @endcode
     */
    bool is_valid() const;
    
    /**
     * @brief 获取连接创建时间
     * 
     * 返回连接建立的时间点，用于计算连接存活时间。
     * 
     * @return 创建时间点
     * 
     * @note 使用 std::chrono::steady_clock 确保时间单调递增
     * 
     * @example
     * @code
     * auto age = std::chrono::steady_clock::now() - conn.created_at();
     * if (age > std::chrono::hours(1)) {
     *     // 连接存活超过 1 小时，可能需要重建
     * }
     * @endcode
     */
    std::chrono::steady_clock::time_point created_at() const {
        return created_at_;
    }
    
    /**
     * @brief 获取最后使用时间
     * 
     * 返回连接最后一次被使用的时间点，用于空闲超时检测。
     * 
     * @return 最后使用时间点
     * 
     * **更新时机：**
     * - 每次调用 send() 或 recv() 时更新
     * - 用于连接池的空闲超时清理
     * 
     * @note 使用 std::chrono::steady_clock 确保时间单调递增
     * 
     * @example
     * @code
     * auto idle_time = std::chrono::steady_clock::now() - conn.last_used_at();
     * if (idle_time > std::chrono::seconds(60)) {
     *     // 连接空闲超过 60 秒，应该关闭
     *     pool.remove_connection(conn);
     * }
     * @endcode
     */
    std::chrono::steady_clock::time_point last_used_at() const {
        return last_used_at_;
    }
    
    /**
     * @brief 获取 socket 文件描述符
     * 
     * @return socket 文件描述符
     * 
     * @note 仅供内部使用，外部代码通常不需要直接访问 fd
     */
    int fd() const { return fd_; }
    
    /**
     * @brief 获取服务实例信息
     * 
     * @return 服务实例引用
     */
    const ServiceInstance& instance() const { return instance_; }

private:
    int fd_;                                                ///< socket 文件描述符
    ServiceInstance instance_;                              ///< 服务实例信息
    NetworkEngine* engine_;                                 ///< 网络引擎指针
    std::chrono::steady_clock::time_point created_at_;      ///< 创建时间
    std::chrono::steady_clock::time_point last_used_at_;    ///< 最后使用时间
};

/**
 * @brief 连接池配置
 * 
 * 定义连接池的行为参数，包括连接数限制和超时设置。
 * 
 * @note 所有配置参数都有合理的默认值
 * @note 可以根据实际场景调整参数以优化性能
 */
struct PoolConfig {
    size_t min_connections = 1;                             ///< 最小连接数（每个实例）
    size_t max_connections = 100;                           ///< 最大连接数（每个实例）
    std::chrono::seconds idle_timeout{60};                  ///< 空闲超时时间
    std::chrono::seconds cleanup_interval{30};              ///< 清理间隔
};

/**
 * @brief 连接池统计信息
 * 
 * 提供连接池的运行状态和性能指标。
 * 
 * @note 用于监控和调试
 */
struct PoolStats {
    size_t total_connections = 0;                           ///< 总连接数
    size_t idle_connections = 0;                            ///< 空闲连接数
    size_t active_connections = 0;                          ///< 活跃连接数
    double connection_reuse_rate = 0.0;                     ///< 连接复用率
};

/**
 * @brief 连接池管理器
 * 
 * 维护到每个服务实例的连接集合，支持连接复用、自动清理和健康检测。
 * 
 * **核心功能：**
 * 
 * 1. **连接复用**：
 *    - 优先返回空闲连接，避免重复建立连接
 *    - 降低 TCP 握手开销，提高性能
 * 
 * 2. **动态扩展**：
 *    - 无空闲连接时自动创建新连接（不超过最大连接数）
 *    - 达到最大连接数时等待或返回错误
 * 
 * 3. **自动清理**：
 *    - 定期清理空闲超时的连接
 *    - 自动移除错误连接
 * 
 * 4. **线程安全**：
 *    - 使用读写锁保护连接池状态
 *    - 支持多线程并发访问
 * 
 * **使用场景：**
 * 
 * ConnectionPool 通常在 RPC 客户端中使用，管理到所有服务实例的连接：
 * 
 * @code
 * PoolConfig config{
 *     .min_connections = 1,
 *     .max_connections = 100,
 *     .idle_timeout = std::chrono::seconds(60)
 * };
 * ConnectionPool pool(config, &engine);
 * 
 * // 获取连接
 * ServiceInstance instance{"127.0.0.1", 8080};
 * auto conn = co_await pool.get_connection(instance);
 * 
 * // 使用连接...
 * 
 * // 归还连接
 * pool.return_connection(std::move(conn));
 * 
 * // 定期清理
 * pool.cleanup_idle_connections();
 * @endcode
 * 
 * @note 线程安全：所有公共方法都是线程安全的
 * @note 性能：使用读写锁，读操作（get_stats）不会阻塞其他读操作
 * @note 资源管理：析构时自动关闭所有连接
 */
class ConnectionPool {
public:
    /**
     * @brief 构造函数
     * 
     * @param config 连接池配置
     * @param engine 网络引擎指针
     * 
     * @note engine 指针必须在 ConnectionPool 生命周期内保持有效
     */
    explicit ConnectionPool(const PoolConfig& config, NetworkEngine* engine);
    
    /**
     * @brief 析构函数 - 清理所有连接
     * 
     * 自动关闭所有连接并释放资源。
     */
    ~ConnectionPool();
    
    // 禁止拷贝和移动
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;
    
    /**
     * @brief 获取到指定实例的连接（协程方法）
     * 
     * 从连接池获取一个到指定服务实例的连接。
     * 优先返回空闲连接，如果没有空闲连接且未达到最大连接数，则创建新连接。
     * 
     * @param instance 服务实例
     * @return RpcTask<Connection> 连接对象
     * 
     * **获取策略：**
     * 1. 检查是否有空闲连接，如果有则返回（优先复用）
     * 2. 如果没有空闲连接且未达到最大连接数，创建新连接
     * 3. 如果达到最大连接数，等待或返回错误
     * 
     * **线程安全：**
     * - 使用读写锁保护连接池状态
     * - 多个线程可以并发获取连接
     * 
     * @throws NetworkException 如果连接失败
     * @throws ResourceExhaustedException 如果达到最大连接数且无法等待
     * 
     * @example
     * @code
     * ServiceInstance instance{"127.0.0.1", 8080};
     * auto conn = co_await pool.get_connection(instance);
     * 
     * // 使用连接...
     * auto result = co_await conn.send(data);
     * 
     * // 归还连接
     * pool.return_connection(std::move(conn));
     * @endcode
     */
    RpcTask<Connection> get_connection(const ServiceInstance& instance);
    
    /**
     * @brief 归还连接
     * 
     * 将连接归还到连接池。如果连接仍然有效，标记为空闲状态；
     * 如果连接已损坏，关闭并移除。
     * 
     * @param conn 连接对象
     * 
     * **归还策略：**
     * 1. 检查连接是否有效（is_valid()）
     * 2. 如果有效，标记为空闲并放回连接池
     * 3. 如果无效，关闭连接并从连接池移除
     * 
     * **线程安全：**
     * - 使用写锁保护连接池状态
     * 
     * @note 归还后，conn 对象将失效（移动语义）
     * @note 如果连接已损坏，会自动关闭并移除
     * 
     * @example
     * @code
     * auto conn = co_await pool.get_connection(instance);
     * 
     * try {
     *     auto result = co_await conn.send(data);
     *     // 成功，归还连接
     *     pool.return_connection(std::move(conn));
     * } catch (const NetworkException& e) {
     *     // 失败，连接已损坏，归还时会自动移除
     *     pool.return_connection(std::move(conn));
     * }
     * @endcode
     */
    void return_connection(Connection conn);
    
    /**
     * @brief 移除到指定实例的所有连接
     * 
     * 关闭并移除到指定服务实例的所有连接（包括空闲和活跃连接）。
     * 
     * @param instance 服务实例
     * 
     * **使用场景：**
     * - 服务实例下线或不健康
     * - 需要强制重建所有连接
     * 
     * **线程安全：**
     * - 使用写锁保护连接池状态
     * 
     * @note 此操作会关闭所有连接，包括正在使用的连接
     * @note 调用者应该确保没有协程正在使用这些连接
     * 
     * @example
     * @code
     * // 服务实例不健康，移除所有连接
     * pool.remove_instance(instance);
     * @endcode
     */
    void remove_instance(const ServiceInstance& instance);
    
    /**
     * @brief 获取连接池统计信息
     * 
     * 返回连接池的运行状态和性能指标。
     * 
     * @return PoolStats 统计信息
     * 
     * **统计指标：**
     * - total_connections：总连接数
     * - idle_connections：空闲连接数
     * - active_connections：活跃连接数
     * - connection_reuse_rate：连接复用率
     * 
     * **线程安全：**
     * - 使用读锁保护连接池状态
     * - 多个线程可以并发读取统计信息
     * 
     * @example
     * @code
     * auto stats = pool.get_stats();
     * std::cout << "Total: " << stats.total_connections << std::endl;
     * std::cout << "Idle: " << stats.idle_connections << std::endl;
     * std::cout << "Active: " << stats.active_connections << std::endl;
     * std::cout << "Reuse rate: " << stats.connection_reuse_rate << std::endl;
     * @endcode
     */
    PoolStats get_stats() const;
    
    /**
     * @brief 清理空闲超时的连接
     * 
     * 遍历所有连接池，关闭并移除空闲时间超过阈值的连接。
     * 
     * **清理策略：**
     * 1. 遍历所有服务实例的连接池
     * 2. 检查每个空闲连接的空闲时间
     * 3. 如果空闲时间超过 idle_timeout，关闭并移除
     * 4. 如果连接已损坏（is_valid() 返回 false），关闭并移除
     * 
     * **调用时机：**
     * - 定期调用（如每 30 秒）
     * - 可以在单独的线程或定时器中调用
     * 
     * **线程安全：**
     * - 使用写锁保护连接池状态
     * 
     * @note 此方法可能会阻塞一段时间（遍历所有连接）
     * @note 建议在低负载时调用，避免影响性能
     * 
     * @example
     * @code
     * // 在单独的线程中定期清理
     * std::thread cleanup_thread([&pool]() {
     *     while (running) {
     *         std::this_thread::sleep_for(std::chrono::seconds(30));
     *         pool.cleanup_idle_connections();
     *     }
     * });
     * @endcode
     */
    void cleanup_idle_connections();

private:
    /**
     * @brief 创建新连接（协程方法）
     * 
     * 建立到指定服务实例的新 TCP 连接。
     * 
     * @param instance 服务实例
     * @return RpcTask<Connection> 新连接对象
     * 
     * @throws NetworkException 如果连接失败
     */
    RpcTask<Connection> create_connection(const ServiceInstance& instance);
    
    /**
     * @brief 实例连接池
     * 
     * 维护到单个服务实例的所有连接。
     */
    struct InstancePool {
        std::vector<Connection> idle_connections;       ///< 空闲连接列表
        std::vector<Connection> active_connections;     ///< 活跃连接列表
        size_t total_count = 0;                         ///< 总连接数
    };
    
    PoolConfig config_;                                 ///< 连接池配置
    NetworkEngine* engine_;                             ///< 网络引擎指针
    std::unordered_map<ServiceInstance, InstancePool, ServiceInstanceHash> pools_;  ///< 实例 -> 连接池映射
    mutable std::shared_mutex mutex_;                   ///< 读写锁
};

}  // namespace frpc
