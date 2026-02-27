/**
 * @file rpc_client.h
 * @brief FRPC 框架 RPC 客户端定义
 * 
 * 定义简化的 MVP RPC 客户端，用于向 RPC 服务端发起远程调用。
 * 此实现专注于核心 RPC 客户端逻辑，跳过网络 I/O 和协程的复杂性。
 */

#ifndef FRPC_RPC_CLIENT_H
#define FRPC_RPC_CLIENT_H

#include "data_models.h"
#include "serializer.h"
#include "error_codes.h"
#include "exceptions.h"
#include <functional>
#include <memory>
#include <optional>
#include <mutex>

namespace frpc {

/**
 * @brief 客户端统计信息
 * 
 * 收集 RPC 客户端的调用统计数据，用于监控和性能分析。
 * 
 * @note 满足需求 11.4
 */
struct ClientStats {
    uint64_t total_calls = 0;             // 总调用数
    uint64_t successful_calls = 0;        // 成功调用数
    uint64_t failed_calls = 0;            // 失败调用数
    uint64_t timeout_calls = 0;           // 超时调用数（MVP 中未实现超时）
    double avg_latency_ms = 0.0;          // 平均延迟（毫秒）
};

/**
 * @brief 传输函数类型
 * 
 * 用于模拟网络传输的函数类型。在 MVP 实现中，我们使用函数对象
 * 来模拟网络发送和接收，避免实际的网络 I/O 复杂性。
 * 
 * 完整实现中，这将被 NetworkEngine 的异步 I/O 操作替代。
 * 
 * @param request_bytes 序列化的请求数据
 * @return 序列化的响应数据，如果传输失败则返回空
 */
using TransportFunction = std::function<std::optional<ByteBuffer>(const ByteBuffer&)>;

/**
 * @brief 简化的 MVP RPC 客户端
 * 
 * 这是一个最小但功能完整的 RPC 客户端，演示核心 RPC 客户端逻辑，
 * 不涉及网络引擎或协程的复杂性。
 * 
 * ## 功能特性
 * 
 * - **请求创建**: 构建 RPC 请求对象
 * - **请求序列化**: 将请求对象序列化为字节流
 * - **网络发送**: 通过传输函数发送请求（模拟）
 * - **响应接收**: 接收响应字节流
 * - **响应反序列化**: 将字节流反序列化为响应对象
 * - **结果返回**: 返回结果给调用者
 * - **错误处理**: 处理网络错误和反序列化错误
 * 
 * ## 使用示例
 * 
 * @code
 * // 创建客户端，提供传输函数
 * RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
 *     // 模拟网络传输：直接调用服务端
 *     return server.handle_raw_request(request);
 * });
 * 
 * // 调用远程服务
 * ByteBuffer payload = serialize_args(1, 2);
 * auto result = client.call("add", payload);
 * 
 * if (result) {
 *     std::cout << "Success: " << result->error_code << std::endl;
 * } else {
 *     std::cout << "Call failed" << std::endl;
 * }
 * @endcode
 * 
 * ## MVP 简化说明
 * 
 * 此 MVP 实现跳过了以下复杂性：
 * - **网络 I/O**: 使用传输函数模拟，而非实际的 socket 操作
 * - **协程**: 使用同步调用，而非 co_await 异步操作
 * - **连接池**: 不管理连接，每次调用都"创建新连接"
 * - **服务发现**: 不查询服务注册中心
 * - **负载均衡**: 不选择服务实例
 * - **超时控制**: 不实现超时机制
 * - **重试机制**: 不自动重试失败的调用
 * 
 * 完整实现将包含所有这些特性。
 * 
 * @note 这是 MVP 实现。完整实现将使用：
 * - NetworkEngine 进行异步 I/O
 * - 协程进行并发请求处理
 * - ConnectionPool 管理连接
 * - ServiceRegistry 进行服务发现
 * - LoadBalancer 进行负载均衡
 */
class RpcClient {
public:
    /**
     * @brief 构造 RPC 客户端
     * 
     * @param transport 传输函数，用于发送请求和接收响应
     * 
     * @example
     * @code
     * RpcClient client([](const ByteBuffer& req) {
     *     // 模拟网络传输
     *     return send_over_network(req);
     * });
     * @endcode
     */
    explicit RpcClient(TransportFunction transport);

    /**
     * @brief 析构函数
     */
    ~RpcClient();

    /**
     * @brief 调用远程服务
     * 
     * 这是客户端的主要入口点，执行完整的 RPC 调用流程：
     * 1. 创建请求对象（生成唯一请求 ID）
     * 2. 序列化请求
     * 3. 通过传输函数发送请求
     * 4. 接收响应
     * 5. 反序列化响应
     * 6. 返回结果
     * 
     * @param service_name 服务名称
     * @param payload 序列化的参数数据
     * @param timeout 超时时间（MVP 中未实现，保留接口）
     * @return std::optional<Response> 响应对象，如果调用失败则返回空
     * 
     * @example
     * @code
     * ByteBuffer args = serialize_args(42, "hello");
     * auto response = client.call("my_service", args);
     * 
     * if (response && response->error_code == ErrorCode::Success) {
     *     auto result = deserialize_result(response->payload);
     *     std::cout << "Result: " << result << std::endl;
     * } else if (response) {
     *     std::cerr << "Error: " << response->error_message << std::endl;
     * } else {
     *     std::cerr << "Network error" << std::endl;
     * }
     * @endcode
     * 
     * ## 错误处理
     * 
     * - **网络传输失败**: 返回空 optional
     * - **响应反序列化失败**: 返回空 optional
     * - **服务端错误**: 返回包含错误码和错误消息的响应对象
     * 
     * ## 满足的需求
     * 
     * - **需求 5.1**: 提供远程服务调用接口
     * - **需求 5.2**: 序列化请求参数
     * - **需求 5.3**: 通过网络发送请求（模拟）
     * - **需求 5.4**: 反序列化响应数据
     * - **需求 5.5**: 返回结果给调用者
     * - **需求 5.6**: 处理网络错误
     * - **需求 5.7**: 处理反序列化错误
     */
    std::optional<Response> call(
        const std::string& service_name,
        const ByteBuffer& payload,
        Duration timeout = std::chrono::milliseconds(5000)
    );

    /**
     * @brief 调用远程服务（带元数据）
     * 
     * 与 call() 类似，但允许指定额外的元数据。
     * 
     * @param service_name 服务名称
     * @param payload 序列化的参数数据
     * @param metadata 元数据键值对
     * @param timeout 超时时间（MVP 中未实现，保留接口）
     * @return std::optional<Response> 响应对象，如果调用失败则返回空
     * 
     * @example
     * @code
     * std::unordered_map<std::string, std::string> metadata;
     * metadata["trace_id"] = "abc-123";
     * metadata["user_id"] = "42";
     * 
     * auto response = client.call_with_metadata("my_service", args, metadata);
     * @endcode
     */
    std::optional<Response> call_with_metadata(
        const std::string& service_name,
        const ByteBuffer& payload,
        const std::unordered_map<std::string, std::string>& metadata,
        Duration timeout = std::chrono::milliseconds(5000)
    );

    /**
     * @brief 获取最后一次调用的请求 ID
     * 
     * 用于调试和追踪。
     * 
     * @return uint32_t 最后一次调用的请求 ID，如果没有调用过则返回 0
     */
    uint32_t last_request_id() const;

    /**
     * @brief 设置最大重试次数
     * 
     * @param max_retries 最大重试次数（默认为 3）
     */
    void set_max_retries(size_t max_retries);

    /**
     * @brief 获取最大重试次数
     * 
     * @return size_t 最大重试次数
     */
    size_t get_max_retries() const;

    /**
     * @brief 获取客户端统计信息
     * 
     * 返回客户端的调用统计数据，包括：
     * - 总调用数
     * - 成功/失败/超时调用数
     * - 平均延迟
     * 
     * @return ClientStats 统计信息结构体
     * 
     * @example
     * @code
     * auto stats = client.get_stats();
     * std::cout << "Total calls: " << stats.total_calls << std::endl;
     * std::cout << "Success rate: " 
     *           << (100.0 * stats.successful_calls / stats.total_calls) << "%" << std::endl;
     * std::cout << "Avg latency: " << stats.avg_latency_ms << "ms" << std::endl;
     * @endcode
     * 
     * @note 满足需求 11.4
     */
    ClientStats get_stats() const;

    /**
     * @brief 重置统计信息
     * 
     * 将所有统计计数器重置为零。
     */
    void reset_stats();

private:
    /**
     * @brief 创建请求对象
     * 
     * @param service_name 服务名称
     * @param payload 参数数据
     * @param metadata 元数据
     * @param timeout 超时时间
     * @return Request 请求对象
     */
    Request create_request(
        const std::string& service_name,
        const ByteBuffer& payload,
        const std::unordered_map<std::string, std::string>& metadata,
        Duration timeout
    );

    /**
     * @brief 执行 RPC 调用的核心逻辑
     * 
     * @param request 请求对象
     * @return std::optional<Response> 响应对象
     */
    std::optional<Response> do_call(const Request& request);

    /**
     * @brief 执行带重试的 RPC 调用
     * 
     * 实现自动重试机制：
     * - 网络传输失败时自动重试
     * - 可配置重试次数
     * - 记录详细的错误日志
     * 
     * @param request 请求对象
     * @return std::optional<Response> 响应对象
     * 
     * @note 满足需求 5.6, 12.1, 12.3
     */
    std::optional<Response> do_call_with_retry(const Request& request);

    /**
     * @brief 记录调用统计信息
     * 
     * @param success 调用是否成功
     * @param latency_ms 调用延迟（毫秒）
     */
    void record_call_stats(bool success, double latency_ms);

    TransportFunction transport_;
    std::unique_ptr<Serializer> serializer_;
    uint32_t last_request_id_;
    size_t max_retries_;  // 最大重试次数
    
    // 统计信息（需求 11.4）
    mutable std::mutex stats_mutex_;  // 保护统计信息的互斥锁
    ClientStats stats_;
    double total_latency_ms_;  // 累计延迟，用于计算平均值
};

} // namespace frpc

#endif // FRPC_RPC_CLIENT_H
