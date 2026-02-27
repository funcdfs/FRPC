/**
 * @file data_models.h
 * @brief FRPC 框架数据模型定义
 * 
 * 定义 RPC 通信中使用的核心数据结构，包括请求对象和响应对象。
 * 这些数据模型是客户端和服务端之间通信的基础。
 */

#ifndef FRPC_DATA_MODELS_H
#define FRPC_DATA_MODELS_H

#include "types.h"
#include "error_codes.h"
#include <atomic>

namespace frpc {

/**
 * @brief RPC 请求对象
 * 
 * 封装一次 RPC 调用的所有信息，包括请求 ID、服务名称、参数数据、
 * 超时时间和元数据。请求对象在客户端创建，序列化后通过网络发送到服务端。
 * 
 * @note 请求 ID 用于匹配请求和响应，确保在并发场景下能够正确关联
 * @note payload 字段存储序列化后的参数数据，具体格式由序列化器决定
 * @note metadata 可用于传递追踪信息（如 trace_id）、认证令牌等
 * 
 * @example 创建请求对象
 * @code
 * Request request;
 * request.request_id = Request::generate_id();
 * request.service_name = "user_service";
 * request.payload = serialize_args(user_id, action);
 * request.timeout = std::chrono::milliseconds(3000);
 * request.metadata["trace_id"] = "abc-123";
 * @endcode
 */
struct Request {
    /**
     * @brief 请求 ID
     * 
     * 唯一标识一次 RPC 调用，用于匹配请求和响应。
     * 在并发场景下，多个请求可能同时在处理，通过请求 ID
     * 可以确保响应被正确路由到对应的请求处理协程。
     * 
     * 使用 generate_id() 静态方法生成唯一 ID。
     */
    uint32_t request_id;
    
    /**
     * @brief 服务名称
     * 
     * 要调用的远程服务的名称，服务端根据此名称路由到对应的处理函数。
     * 服务名称应该在服务注册时定义，客户端和服务端必须使用相同的名称。
     * 
     * @example "user_service", "order_service", "payment_service"
     */
    std::string service_name;
    
    /**
     * @brief 序列化后的参数数据
     * 
     * 存储服务调用参数的序列化字节流。具体的序列化格式由
     * Serializer 实现决定（如二进制、JSON、Protobuf 等）。
     * 
     * 服务端接收到请求后，会根据服务定义反序列化 payload
     * 以获取实际的参数值。
     */
    ByteBuffer payload;
    
    /**
     * @brief 超时时间
     * 
     * 指定此次 RPC 调用的最大等待时间。如果在超时时间内
     * 未收到响应，客户端将取消请求并返回超时错误。
     * 
     * 默认值可以在客户端配置中设置，也可以为每次调用单独指定。
     * 
     * @note 超时时间从请求发送开始计算，包括网络传输和服务处理时间
     */
    Duration timeout;
    
    /**
     * @brief 元数据
     * 
     * 存储额外的键值对信息，用于传递非业务数据，如：
     * - trace_id: 分布式追踪 ID
     * - auth_token: 认证令牌
     * - client_version: 客户端版本
     * - request_time: 请求发起时间
     * 
     * 元数据会随请求一起序列化和传输，服务端可以读取和使用这些信息。
     */
    std::unordered_map<std::string, std::string> metadata;
    
    /**
     * @brief 生成唯一的请求 ID
     * 
     * 使用原子递增计数器生成全局唯一的请求 ID。
     * 此方法是线程安全的，可以在多线程环境下并发调用。
     * 
     * @return 唯一的请求 ID
     * 
     * @note ID 从 1 开始递增，0 保留为无效 ID
     * @note 在单个进程生命周期内保证唯一性
     * 
     * @example
     * @code
     * Request request;
     * request.request_id = Request::generate_id();
     * @endcode
     */
    static uint32_t generate_id();
};

/**
 * @brief RPC 响应对象
 * 
 * 封装 RPC 调用的结果，包括对应的请求 ID、错误码、错误消息、
 * 返回值数据和元数据。响应对象在服务端创建，序列化后通过网络
 * 发送回客户端。
 * 
 * @note 响应的 request_id 必须与请求的 request_id 匹配
 * @note error_code 为 Success 时表示调用成功，payload 包含返回值
 * @note error_code 非 Success 时表示调用失败，error_message 包含错误描述
 * 
 * @example 创建成功响应
 * @code
 * Response response;
 * response.request_id = request.request_id;
 * response.error_code = ErrorCode::Success;
 * response.payload = serialize_result(result);
 * @endcode
 * 
 * @example 创建错误响应
 * @code
 * Response response;
 * response.request_id = request.request_id;
 * response.error_code = ErrorCode::ServiceNotFound;
 * response.error_message = "Service 'unknown_service' not found";
 * @endcode
 */
struct Response {
    /**
     * @brief 对应的请求 ID
     * 
     * 标识此响应对应的请求。客户端通过匹配 request_id
     * 将响应路由到正确的等待协程。
     * 
     * @note 必须与原始请求的 request_id 完全一致
     */
    uint32_t request_id;
    
    /**
     * @brief 错误码
     * 
     * 指示 RPC 调用的执行结果：
     * - ErrorCode::Success (0): 调用成功，payload 包含返回值
     * - 其他错误码: 调用失败，error_message 包含错误描述
     * 
     * 客户端应该首先检查 error_code，只有在成功时才解析 payload。
     * 
     * @see ErrorCode 枚举定义了所有可能的错误类型
     */
    ErrorCode error_code;
    
    /**
     * @brief 错误描述
     * 
     * 当 error_code 非 Success 时，此字段包含人类可读的错误描述信息。
     * 错误描述应该清晰地说明失败原因，帮助开发者快速定位问题。
     * 
     * 当 error_code 为 Success 时，此字段通常为空。
     * 
     * @example "Service 'user_service' not found"
     * @example "Connection timeout after 5000ms"
     * @example "Invalid argument: user_id must be positive"
     */
    std::string error_message;
    
    /**
     * @brief 序列化后的返回值数据
     * 
     * 当 error_code 为 Success 时，存储服务调用返回值的序列化字节流。
     * 具体的序列化格式由 Serializer 实现决定。
     * 
     * 当 error_code 非 Success 时，此字段通常为空。
     * 
     * 客户端接收到响应后，会反序列化 payload 以获取实际的返回值。
     */
    ByteBuffer payload;
    
    /**
     * @brief 元数据
     * 
     * 存储额外的键值对信息，用于传递非业务数据，如：
     * - server_version: 服务端版本
     * - process_time_ms: 服务处理耗时（毫秒）
     * - server_id: 处理请求的服务器 ID
     * 
     * 元数据会随响应一起序列化和传输，客户端可以读取这些信息
     * 用于监控、调试或追踪。
     */
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief 服务器统计信息
 * 
 * 收集和报告 RPC 服务器的运行时统计数据，用于监控服务器性能和健康状况。
 * 这些指标可以帮助识别性能瓶颈、监控服务质量和进行容量规划。
 * 
 * @note 所有计数器字段都是累积值，从服务器启动开始计数
 * @note 延迟和 QPS 指标是基于滑动窗口计算的实时值
 * 
 * @example 获取和使用统计信息
 * @code
 * RpcServer server;
 * // ... 处理一些请求 ...
 * auto stats = server.get_stats();
 * std::cout << "Total requests: " << stats.total_requests << std::endl;
 * std::cout << "Success rate: " 
 *           << (100.0 * stats.successful_requests / stats.total_requests) 
 *           << "%" << std::endl;
 * std::cout << "QPS: " << stats.qps << std::endl;
 * std::cout << "P99 latency: " << stats.p99_latency_ms << "ms" << std::endl;
 * @endcode
 * 
 * @see RpcServer::get_stats()
 * @see Requirements 11.4, 15.1
 */
struct ServerStats {
    /**
     * @brief 总请求数
     * 
     * 服务器接收到的所有请求的总数，包括成功和失败的请求。
     * 此计数器从服务器启动开始累积，不会重置。
     */
    uint64_t total_requests = 0;
    
    /**
     * @brief 成功请求数
     * 
     * 成功处理的请求数量（error_code == Success）。
     * 成功意味着请求被正确反序列化、路由到处理函数、执行完成并返回成功响应。
     */
    uint64_t successful_requests = 0;
    
    /**
     * @brief 失败请求数
     * 
     * 失败的请求数量（error_code != Success）。
     * 失败包括：
     * - 反序列化失败 (DeserializationError)
     * - 服务未找到 (ServiceNotFound)
     * - 服务执行异常 (ServiceException)
     * - 其他错误
     * 
     * @note total_requests = successful_requests + failed_requests
     */
    uint64_t failed_requests = 0;
    
    /**
     * @brief 活跃连接数
     * 
     * 当前正在处理请求的连接数。
     * 
     * @note MVP 实现中此字段始终为 0，因为 MVP 版本不包含网络引擎和连接管理。
     *       完整实现会跟踪实际的网络连接数。
     */
    uint64_t active_connections = 0;
    
    /**
     * @brief 平均延迟（毫秒）
     * 
     * 所有请求的平均处理延迟，单位为毫秒。
     * 延迟从请求开始处理到响应生成完成的时间。
     * 
     * 计算方式：所有请求延迟的算术平均值。
     * 
     * @note 此指标受异常值影响较大，建议结合 P99 延迟一起查看
     */
    double avg_latency_ms = 0.0;
    
    /**
     * @brief P99 延迟（毫秒）
     * 
     * 99% 的请求处理延迟低于此值，单位为毫秒。
     * P99 延迟是衡量服务质量的重要指标，反映了大多数用户的体验。
     * 
     * 计算方式：对所有请求延迟排序，取第 99 百分位的值。
     * 
     * @note P99 延迟比平均延迟更能反映真实的用户体验，因为它不受极端异常值的影响
     * @note 目标：在局域网环境下 P99 延迟应小于 10ms
     */
    double p99_latency_ms = 0.0;
    
    /**
     * @brief 每秒查询数 (QPS)
     * 
     * 服务器每秒处理的请求数量，反映服务器的吞吐能力。
     * 
     * 计算方式：基于最近一段时间（如最近 1 秒或 10 秒）的请求数计算。
     * 使用滑动窗口算法，提供实时的 QPS 值。
     * 
     * @note 目标：在标准硬件配置下 QPS 应达到至少 50,000
     * @note QPS 会随负载变化而波动，建议观察一段时间的平均值
     */
    double qps = 0.0;
};

} // namespace frpc

#endif // FRPC_DATA_MODELS_H
