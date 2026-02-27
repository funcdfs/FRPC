/**
 * @file config.h
 * @brief FRPC 框架配置管理
 * 
 * 定义框架的各种配置结构，包括服务器配置、客户端配置、连接池配置和健康检测配置。
 * 支持从配置文件加载配置，并在配置文件无效时使用默认值。
 */

#ifndef FRPC_CONFIG_H
#define FRPC_CONFIG_H

#include "types.h"
#include <string>
#include <chrono>
#include <cstdint>

namespace frpc {

/**
 * @brief 服务器配置
 * 
 * 定义 RPC 服务器的运行参数，包括监听地址、端口、连接限制和线程配置。
 * 
 * @note 所有配置项都有合理的默认值，可以直接使用默认配置启动服务器
 * @note 建议根据实际硬件资源和业务需求调整配置参数
 * 
 * @example 使用默认配置
 * @code
 * ServerConfig config;
 * RpcServer server(config);
 * server.start();
 * @endcode
 * 
 * @example 从配置文件加载
 * @code
 * auto config = ServerConfig::load_from_file("server_config.json");
 * RpcServer server(config);
 * server.start();
 * @endcode
 */
struct ServerConfig {
    /**
     * @brief 监听地址
     * 
     * 服务器绑定的 IP 地址。
     * 
     * 推荐值：
     * - "0.0.0.0": 监听所有网络接口（默认）
     * - "127.0.0.1": 仅监听本地回环接口（用于本地测试）
     * - 具体 IP: 监听指定网络接口
     * 
     * @note 使用 "0.0.0.0" 时，服务器可以从任何网络接口接收连接
     * @note 生产环境建议配合防火墙规则使用
     */
    std::string listen_addr = "0.0.0.0";
    
    /**
     * @brief 监听端口
     * 
     * 服务器监听的 TCP 端口号。
     * 
     * 推荐值：
     * - 8080: 默认端口，适合开发和测试
     * - 1024-65535: 用户端口范围，避免使用系统保留端口（0-1023）
     * 
     * @note 确保端口未被其他程序占用
     * @note Linux 下绑定 1024 以下端口需要 root 权限
     */
    uint16_t listen_port = 8080;
    
    /**
     * @brief 最大连接数
     * 
     * 服务器同时接受的最大客户端连接数。达到此限制后，新的连接请求将被拒绝。
     * 
     * 推荐值：
     * - 开发环境: 100-1000
     * - 生产环境: 5000-10000（默认）
     * - 高负载环境: 10000-50000
     * 
     * @note 需要考虑系统的文件描述符限制（ulimit -n）
     * @note 每个连接会占用一定的内存资源（约 4-8KB）
     * @note 建议根据服务器内存和 CPU 资源调整此值
     */
    size_t max_connections = 10000;
    
    /**
     * @brief 连接空闲超时时间
     * 
     * 客户端连接在无数据传输的情况下保持打开的最长时间。
     * 超过此时间后，服务器将主动关闭连接以释放资源。
     * 
     * 推荐值：
     * - 短连接场景: 30-60 秒
     * - 长连接场景: 300 秒（5 分钟，默认）
     * - 持久连接场景: 600-1800 秒（10-30 分钟）
     * 
     * @note 设置过短可能导致频繁的连接建立和断开
     * @note 设置过长会占用更多服务器资源
     * @note 客户端应该实现心跳机制以保持长连接
     */
    std::chrono::seconds idle_timeout{300};
    
    /**
     * @brief 工作线程数
     * 
     * 用于处理网络 I/O 的工作线程数量。
     * 
     * 推荐值：
     * - 0 或 1: 单线程模式（默认），适合低并发场景
     * - CPU 核心数: 充分利用多核 CPU
     * - CPU 核心数 * 2: 高并发场景
     * 
     * @note 0 和 1 都表示单线程模式
     * @note 多线程模式下，每个线程运行独立的事件循环
     * @note 线程数不是越多越好，需要根据实际负载测试确定最优值
     * @note 建议不超过 CPU 核心数的 2 倍
     */
    size_t worker_threads = 1;
    
    /**
     * @brief 从配置文件加载服务器配置
     * 
     * 从 JSON 格式的配置文件中加载服务器配置参数。
     * 如果文件不存在、格式无效或解析失败，将返回默认配置。
     * 
     * @param path 配置文件路径（相对路径或绝对路径）
     * @return 服务器配置对象
     * 
     * @note 配置文件格式为 JSON，示例：
     * @code{.json}
     * {
     *   "listen_addr": "0.0.0.0",
     *   "listen_port": 8080,
     *   "max_connections": 10000,
     *   "idle_timeout_seconds": 300,
     *   "worker_threads": 4
     * }
     * @endcode
     * 
     * @note 如果配置文件中缺少某些字段，将使用默认值
     * @note 如果配置文件无效，会记录警告日志并返回默认配置
     * 
     * @example
     * @code
     * auto config = ServerConfig::load_from_file("config/server.json");
     * if (config.listen_port == 8080) {
     *     // 使用了默认配置或配置文件中指定了 8080
     * }
     * @endcode
     */
    static ServerConfig load_from_file(const std::string& path);
};

/**
 * @brief 客户端配置
 * 
 * 定义 RPC 客户端的行为参数，包括超时设置、重试策略和负载均衡策略。
 * 
 * @note 所有配置项都有合理的默认值
 * @note 可以为每次 RPC 调用单独指定超时时间，覆盖默认配置
 * 
 * @example 使用默认配置
 * @code
 * ClientConfig config;
 * RpcClient client(config);
 * @endcode
 * 
 * @example 自定义配置
 * @code
 * ClientConfig config;
 * config.default_timeout = std::chrono::milliseconds(3000);
 * config.max_retries = 5;
 * config.load_balance_strategy = "least_connection";
 * RpcClient client(config);
 * @endcode
 */
struct ClientConfig {
    /**
     * @brief 默认超时时间
     * 
     * RPC 调用的默认超时时间。如果在此时间内未收到响应，调用将失败并返回超时错误。
     * 
     * 推荐值：
     * - 快速服务: 1000-3000 毫秒
     * - 一般服务: 5000 毫秒（默认）
     * - 慢速服务: 10000-30000 毫秒
     * 
     * @note 超时时间包括网络传输时间和服务处理时间
     * @note 可以在调用时单独指定超时时间，覆盖此默认值
     * @note 设置过短可能导致正常请求超时失败
     * @note 设置过长会影响系统的响应性
     */
    std::chrono::milliseconds default_timeout{5000};
    
    /**
     * @brief 最大重试次数
     * 
     * 当 RPC 调用失败时（网络错误、超时等），自动重试的最大次数。
     * 
     * 推荐值：
     * - 幂等操作: 3-5 次（默认 3 次）
     * - 非幂等操作: 0 次（禁用重试）
     * - 关键操作: 5-10 次
     * 
     * @note 重试仅针对网络层错误，不包括业务逻辑错误
     * @note 非幂等操作（如扣款、下单）应该禁用自动重试
     * @note 重试会增加系统负载，需要谨慎配置
     * @note 建议配合指数退避策略使用
     */
    size_t max_retries = 3;
    
    /**
     * @brief 负载均衡策略
     * 
     * 当有多个服务实例可用时，选择实例的策略。
     * 
     * 支持的策略：
     * - "round_robin": 轮询策略（默认），按顺序依次选择实例
     * - "random": 随机策略，随机选择实例
     * - "least_connection": 最少连接策略，选择当前连接数最少的实例
     * - "weighted_round_robin": 加权轮询策略，根据实例权重按比例选择
     * 
     * 推荐值：
     * - 均匀负载: "round_robin"（默认）
     * - 无状态服务: "random"
     * - 有状态服务: "least_connection"
     * - 异构实例: "weighted_round_robin"
     * 
     * @note 策略名称不区分大小写
     * @note 如果指定了无效的策略名称，将使用默认的轮询策略
     * @note 不同策略适用于不同的场景，需要根据实际情况选择
     */
    std::string load_balance_strategy = "round_robin";
    
    /**
     * @brief 从配置文件加载客户端配置
     * 
     * 从 JSON 格式的配置文件中加载客户端配置参数。
     * 如果文件不存在、格式无效或解析失败，将返回默认配置。
     * 
     * @param path 配置文件路径（相对路径或绝对路径）
     * @return 客户端配置对象
     * 
     * @note 配置文件格式为 JSON，示例：
     * @code{.json}
     * {
     *   "default_timeout_ms": 5000,
     *   "max_retries": 3,
     *   "load_balance_strategy": "round_robin"
     * }
     * @endcode
     * 
     * @note 如果配置文件中缺少某些字段，将使用默认值
     * @note 如果配置文件无效，会记录警告日志并返回默认配置
     * 
     * @example
     * @code
     * auto config = ClientConfig::load_from_file("config/client.json");
     * RpcClient client(config);
     * @endcode
     */
    static ClientConfig load_from_file(const std::string& path);
};

/**
 * @brief 连接池配置
 * 
 * 定义连接池的行为参数，包括连接数限制、空闲超时和清理间隔。
 * 连接池用于复用 TCP 连接，降低连接建立的开销。
 * 
 * @note 合理的连接池配置可以显著提升性能
 * @note 需要根据服务实例数量和并发量调整配置
 * 
 * @example 默认配置
 * @code
 * PoolConfig config;
 * ConnectionPool pool(config);
 * @endcode
 * 
 * @example 高并发配置
 * @code
 * PoolConfig config;
 * config.min_connections = 10;
 * config.max_connections = 500;
 * config.idle_timeout = std::chrono::seconds(120);
 * ConnectionPool pool(config);
 * @endcode
 */
struct PoolConfig {
    /**
     * @brief 最小连接数
     * 
     * 连接池为每个服务实例保持的最小连接数。
     * 即使连接空闲，也不会关闭低于此数量的连接。
     * 
     * 推荐值：
     * - 低并发: 1（默认）
     * - 中并发: 5-10
     * - 高并发: 10-20
     * 
     * @note 最小连接数保证了快速响应，避免频繁建立连接
     * @note 设置过高会占用更多资源
     * @note 建议根据平均并发量设置
     */
    size_t min_connections = 1;
    
    /**
     * @brief 最大连接数
     * 
     * 连接池为每个服务实例允许的最大连接数。
     * 达到此限制后，新的连接请求将等待或失败。
     * 
     * 推荐值：
     * - 低并发: 10-50
     * - 中并发: 100（默认）
     * - 高并发: 200-500
     * 
     * @note 需要考虑服务端的最大连接数限制
     * @note 每个连接会占用一定的系统资源（文件描述符、内存）
     * @note 建议根据峰值并发量设置，留有一定余量
     * @note 总连接数 = 最大连接数 × 服务实例数
     */
    size_t max_connections = 100;
    
    /**
     * @brief 连接空闲超时时间
     * 
     * 连接在空闲状态下保持打开的最长时间。
     * 超过此时间后，连接将被关闭并从连接池移除。
     * 
     * 推荐值：
     * - 短连接场景: 30 秒
     * - 一般场景: 60 秒（默认）
     * - 长连接场景: 120-300 秒
     * 
     * @note 设置过短会导致频繁的连接建立和关闭
     * @note 设置过长会占用更多资源
     * @note 应该小于服务端的空闲超时时间
     * @note 建议根据业务访问模式调整
     */
    std::chrono::seconds idle_timeout{60};
    
    /**
     * @brief 清理间隔
     * 
     * 连接池执行空闲连接清理的时间间隔。
     * 定期扫描连接池，关闭超过空闲超时时间的连接。
     * 
     * 推荐值：
     * - 频繁清理: 10-20 秒
     * - 一般清理: 30 秒（默认）
     * - 低频清理: 60-120 秒
     * 
     * @note 清理间隔不应该大于空闲超时时间
     * @note 设置过短会增加 CPU 开销
     * @note 设置过长会延迟资源释放
     * @note 建议设置为空闲超时时间的 1/2 到 1/3
     */
    std::chrono::seconds cleanup_interval{30};
};

/**
 * @brief 健康检测配置
 * 
 * 定义健康检测器的行为参数，包括检测间隔、超时时间和失败阈值。
 * 健康检测用于及时发现和移除不可用的服务实例。
 * 
 * @note 合理的健康检测配置可以提高系统的可用性
 * @note 需要平衡检测频率和系统开销
 * 
 * @example 默认配置
 * @code
 * HealthCheckConfig config;
 * HealthChecker checker(config, registry);
 * @endcode
 * 
 * @example 快速检测配置
 * @code
 * HealthCheckConfig config;
 * config.interval = std::chrono::seconds(5);
 * config.timeout = std::chrono::seconds(2);
 * config.failure_threshold = 2;
 * HealthChecker checker(config, registry);
 * @endcode
 */
struct HealthCheckConfig {
    /**
     * @brief 健康检测间隔
     * 
     * 两次健康检测之间的时间间隔。
     * 检测器会定期向服务实例发送健康检测请求。
     * 
     * 推荐值：
     * - 快速检测: 5 秒
     * - 一般检测: 10 秒（默认）
     * - 低频检测: 30-60 秒
     * 
     * @note 检测间隔越短，故障发现越快，但系统开销越大
     * @note 检测间隔越长，故障发现越慢，但系统开销越小
     * @note 建议根据业务对可用性的要求调整
     * @note 对于关键服务，建议使用较短的检测间隔
     */
    std::chrono::seconds interval{10};
    
    /**
     * @brief 健康检测超时时间
     * 
     * 单次健康检测请求的超时时间。
     * 如果在此时间内未收到响应，视为检测失败。
     * 
     * 推荐值：
     * - 快速响应: 1-2 秒
     * - 一般响应: 3 秒（默认）
     * - 慢速响应: 5-10 秒
     * 
     * @note 超时时间应该小于检测间隔
     * @note 设置过短可能导致误判（网络抖动时）
     * @note 设置过长会延迟故障发现
     * @note 建议设置为检测间隔的 1/3 到 1/2
     */
    std::chrono::seconds timeout{3};
    
    /**
     * @brief 失败阈值
     * 
     * 连续失败多少次后，将服务实例标记为不健康。
     * 只有达到此阈值，实例才会从服务列表中移除。
     * 
     * 推荐值：
     * - 快速摘除: 2 次
     * - 一般摘除: 3 次（默认）
     * - 保守摘除: 5-10 次
     * 
     * @note 阈值越小，故障摘除越快，但误判风险越高
     * @note 阈值越大，故障摘除越慢，但更能容忍网络抖动
     * @note 建议根据网络稳定性和业务容错能力调整
     * @note 对于网络不稳定的环境，建议使用较大的阈值
     */
    int failure_threshold = 3;
    
    /**
     * @brief 成功阈值
     * 
     * 不健康的实例连续成功多少次后，将其标记为健康。
     * 只有达到此阈值，实例才会重新加入服务列表。
     * 
     * 推荐值：
     * - 快速恢复: 1 次
     * - 一般恢复: 2 次（默认）
     * - 保守恢复: 3-5 次
     * 
     * @note 阈值越小，故障恢复越快，但可能过早恢复未完全恢复的实例
     * @note 阈值越大，故障恢复越慢，但更能确保实例已完全恢复
     * @note 建议根据服务的恢复特性调整
     * @note 对于需要预热的服务，建议使用较大的阈值
     */
    int success_threshold = 2;
};

} // namespace frpc

#endif // FRPC_CONFIG_H
