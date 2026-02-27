# API 参考

本文档提供 FRPC 框架的 API 参考。完整的 API 文档请查看 Doxygen 生成的文档。

## 生成 API 文档

### 前置要求

安装 Doxygen 和 Graphviz：

**Ubuntu/Debian:**
```bash
sudo apt-get install doxygen graphviz
```

**macOS:**
```bash
brew install doxygen graphviz
```

**Fedora/RHEL:**
```bash
sudo dnf install doxygen graphviz
```

### 生成文档

```bash
# 方法 1: 使用提供的脚本
./docs/generate_docs.sh

# 方法 2: 直接使用 doxygen
doxygen Doxyfile
```

生成的文档位于 `docs/html/` 目录，使用浏览器打开 `docs/html/index.html` 查看。

## 核心类概览

### RpcServer

RPC 服务端类，用于注册和处理服务。

```cpp
class RpcServer {
public:
    explicit RpcServer(const ServerConfig& config);
    
    void register_service(const std::string& name, ServiceHandler handler);
    void start();
    void stop();
    ServerStats get_stats() const;
};
```

### RpcClient

RPC 客户端类，用于调用远程服务。

```cpp
class RpcClient {
public:
    explicit RpcClient(const ClientConfig& config);
    
    template<typename R, typename... Args>
    Awaitable<std::optional<R>> call(const std::string& service, Args&&... args);
    
    ClientStats get_stats() const;
};
```

### ServiceRegistry

服务注册中心，管理服务实例。

```cpp
class ServiceRegistry {
public:
    bool register_service(const std::string& name, const ServiceInstance& instance);
    void unregister_service(const std::string& name, const ServiceInstance& instance);
    std::vector<ServiceInstance> get_instances(const std::string& name) const;
    void subscribe(const std::string& name, Callback callback);
};
```

### ConnectionPool

连接池，管理和复用网络连接。

```cpp
class ConnectionPool {
public:
    explicit ConnectionPool(const PoolConfig& config);
    
    Awaitable<Connection> get_connection(const ServiceInstance& instance);
    void return_connection(Connection conn);
    void cleanup_idle_connections();
    PoolStats get_stats() const;
};
```

### LoadBalancer

负载均衡器基类。

```cpp
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;
    virtual ServiceInstance select(const std::vector<ServiceInstance>& instances) = 0;
};
```

### HealthChecker

健康检测器，监控服务实例健康状态。

```cpp
class HealthChecker {
public:
    HealthChecker(const HealthCheckConfig& config, ServiceRegistry* registry);
    
    void start();
    void stop();
    void add_target(const std::string& service, const ServiceInstance& instance);
    void remove_target(const std::string& service, const ServiceInstance& instance);
};
```

### Serializer

序列化器，处理数据序列化和反序列化。

```cpp
class Serializer {
public:
    std::vector<uint8_t> serialize(const Request& request);
    Request deserialize_request(std::span<const uint8_t> data);
    std::vector<uint8_t> serialize(const Response& response);
    Response deserialize_response(std::span<const uint8_t> data);
};
```

## 配置类

### ServerConfig

服务端配置。

```cpp
struct ServerConfig {
    std::string listen_addr = "0.0.0.0";
    uint16_t listen_port = 8080;
    size_t max_connections = 10000;
    std::chrono::seconds idle_timeout{300};
    size_t worker_threads = 1;
    
    static ServerConfig load_from_file(const std::string& filename);
};
```

### ClientConfig

客户端配置。

```cpp
struct ClientConfig {
    std::chrono::milliseconds default_timeout{5000};
    size_t max_retries = 3;
    std::string load_balance_strategy = "round_robin";
    
    static ClientConfig load_from_file(const std::string& filename);
};
```

### PoolConfig

连接池配置。

```cpp
struct PoolConfig {
    size_t min_connections = 1;
    size_t max_connections = 100;
    std::chrono::seconds idle_timeout{60};
    std::chrono::seconds cleanup_interval{30};
};
```

### HealthCheckConfig

健康检测配置。

```cpp
struct HealthCheckConfig {
    std::chrono::seconds interval{10};
    std::chrono::seconds timeout{3};
    int failure_threshold = 3;
    int success_threshold = 2;
};
```

## 数据结构

### Request

RPC 请求对象。

```cpp
struct Request {
    uint32_t request_id;
    std::string service_name;
    ByteBuffer payload;
    Duration timeout;
    std::unordered_map<std::string, std::string> metadata;
    
    static uint32_t generate_id();
};
```

### Response

RPC 响应对象。

```cpp
struct Response {
    uint32_t request_id;
    ErrorCode error_code;
    std::string error_message;
    ByteBuffer payload;
    std::unordered_map<std::string, std::string> metadata;
};
```

### ServiceInstance

服务实例信息。

```cpp
struct ServiceInstance {
    std::string host;
    uint16_t port;
    int weight = 100;
    std::unordered_map<std::string, std::string> metadata;
};
```

## 错误码

### ErrorCode

```cpp
enum class ErrorCode : uint32_t {
    Success = 0,
    
    // 网络错误 (1000-1999)
    NetworkError = 1000,
    ConnectionFailed = 1001,
    ConnectionClosed = 1002,
    SendFailed = 1003,
    RecvFailed = 1004,
    Timeout = 1005,
    
    // 序列化错误 (2000-2999)
    SerializationError = 2000,
    DeserializationError = 2001,
    InvalidFormat = 2002,
    
    // 服务错误 (3000-3999)
    ServiceNotFound = 3000,
    ServiceUnavailable = 3001,
    NoInstanceAvailable = 3002,
    
    // 参数错误 (4000-4999)
    InvalidArgument = 4000,
    MissingArgument = 4001,
    
    // 系统错误 (5000-5999)
    InternalError = 5000,
    OutOfMemory = 5001,
    ResourceExhausted = 5002,
};
```

## 异常类

### FrpcException

所有 FRPC 异常的基类。

```cpp
class FrpcException : public std::exception {
public:
    explicit FrpcException(ErrorCode code, const std::string& message);
    
    ErrorCode code() const noexcept;
    const char* what() const noexcept override;
};
```

### NetworkException

网络相关异常。

```cpp
class NetworkException : public FrpcException {
public:
    explicit NetworkException(ErrorCode code, const std::string& message);
};
```

### SerializationException

序列化相关异常。

```cpp
class SerializationException : public FrpcException {
public:
    explicit SerializationException(ErrorCode code, const std::string& message);
};
```

### ServiceException

服务相关异常。

```cpp
class ServiceException : public FrpcException {
public:
    explicit ServiceException(ErrorCode code, const std::string& message);
};
```

### TimeoutException

超时异常。

```cpp
class TimeoutException : public NetworkException {
public:
    explicit TimeoutException(const std::string& message);
};
```

## 日志系统

### Logger

日志记录器。

```cpp
class Logger {
public:
    static void set_level(LogLevel level);
    static void set_output_file(const std::string& filename);
    static void set_output_console(bool enable);
    
    static void log(LogLevel level, const std::string& message);
};

// 日志宏
LOG_DEBUG(format, ...);
LOG_INFO(format, ...);
LOG_WARN(format, ...);
LOG_ERROR(format, ...);
```

### LogLevel

日志级别。

```cpp
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};
```

## 统计信息

### ServerStats

服务端统计信息。

```cpp
struct ServerStats {
    uint64_t total_requests;
    uint64_t successful_requests;
    uint64_t failed_requests;
    double avg_latency_ms;
    double qps;
    uint64_t active_connections;
};
```

### ClientStats

客户端统计信息。

```cpp
struct ClientStats {
    uint64_t total_calls;
    uint64_t successful_calls;
    uint64_t failed_calls;
    uint64_t timeout_calls;
    double avg_latency_ms;
};
```

### PoolStats

连接池统计信息。

```cpp
struct PoolStats {
    size_t total_connections;
    size_t idle_connections;
    size_t active_connections;
    double connection_reuse_rate;
};
```

## 文档注释规范

FRPC 框架使用 Doxygen 风格的文档注释。

### 类文档注释

```cpp
/**
 * @brief 网络引擎 - 基于 Reactor 模式的事件驱动网络层
 * 
 * 使用 epoll 实现高效的事件多路复用，支持非阻塞 socket 操作。
 * 所有网络 I/O 操作都是异步的，通过回调或协程恢复通知完成。
 * 
 * @note 线程安全：NetworkEngine 不是线程安全的，应在单个线程中使用
 */
class NetworkEngine {
    // ...
};
```

### 函数文档注释

```cpp
/**
 * @brief 异步发送数据（协程友好）
 * 
 * @param fd 文件描述符
 * @param data 要发送的数据
 * @return Awaitable 对象，可用于 co_await
 * 
 * @throws NetworkException 如果 socket 无效或发送失败
 * 
 * @example
 * auto result = co_await engine.async_send(fd, data);
 * if (!result) {
 *     // 处理发送失败
 * }
 */
Awaitable<SendResult> async_send(int fd, std::span<const uint8_t> data);
```

## 验证文档完整性

运行文档验证脚本检查所有公共接口是否有完整的文档注释：

```bash
python3 docs/verify_docs.py
```

## 下一步

- [快速开始](02-quick-start.md) - 快速上手
- [使用指南](04-user-guide.md) - 详细使用说明
- [示例程序](../examples/README.md) - 示例代码
