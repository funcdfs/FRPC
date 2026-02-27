# 使用指南

本指南详细介绍 FRPC 框架的功能和使用方法。

## 目录

1. [核心概念](#核心概念)
2. [RPC 服务端](#rpc-服务端)
3. [RPC 客户端](#rpc-客户端)
4. [服务治理](#服务治理)
5. [协程编程](#协程编程)
6. [错误处理](#错误处理)
7. [性能优化](#性能优化)
8. [线程模型](#线程模型)
9. [扩展开发](#扩展开发)

## 核心概念

### RPC 调用流程

1. **客户端**发起调用，指定服务名称和参数
2. **服务发现**查询可用的服务实例
3. **负载均衡**选择一个服务实例
4. **连接池**获取或创建连接
5. **序列化**将请求参数转换为字节流
6. **网络传输**发送请求到服务端
7. **服务端**接收请求，反序列化参数
8. **服务路由**根据服务名称找到处理函数
9. **执行服务**调用处理函数
10. **序列化响应**将返回值转换为字节流
11. **网络传输**发送响应到客户端
12. **反序列化**将响应转换为返回值
13. **返回结果**给调用者

### 协程模型

FRPC 使用 C++20 无栈协程实现异步操作：

```cpp
// 传统回调方式
void send_data(int fd, const Data& data, Callback callback) {
    async_send(fd, data, [callback](Result result) {
        if (result.success) {
            async_recv(fd, [callback](Data response) {
                callback(response);
            });
        }
    });
}

// 协程方式
RpcTask<Data> send_and_recv(int fd, const Data& data) {
    auto send_result = co_await async_send(fd, data);
    if (!send_result) {
        co_return Data{};
    }
    
    auto response = co_await async_recv(fd);
    co_return response;
}
```

## RPC 服务端

### 基本用法

```cpp
#include <frpc/rpc_server.h>

// 定义服务处理函数
RpcTask<std::string> echo_service(const std::string& message) {
    co_return message;
}

RpcTask<int> add_service(int a, int b) {
    co_return a + b;
}

int main() {
    // 创建服务器
    ServerConfig config;
    config.listen_port = 8080;
    RpcServer server(config);
    
    // 注册服务
    server.register_service("echo", echo_service);
    server.register_service("add", add_service);
    
    // 启动服务器
    server.start();
    
    return 0;
}
```

### 服务注册

服务处理函数必须是协程函数，返回 `RpcTask<T>` 类型：

```cpp
// 无参数服务
RpcTask<std::string> get_version() {
    co_return "1.0.0";
}

// 多参数服务
RpcTask<double> divide(double a, double b) {
    if (b == 0) {
        throw std::invalid_argument("Division by zero");
    }
    co_return a / b;
}

// 复杂类型服务
struct User {
    std::string name;
    int age;
};

RpcTask<User> get_user(int user_id) {
    // 查询数据库...
    User user{"Alice", 30};
    co_return user;
}
```

### 错误处理

服务函数可以抛出异常，框架会自动捕获并返回错误响应：

```cpp
RpcTask<int> risky_operation(int value) {
    if (value < 0) {
        throw std::invalid_argument("Value must be non-negative");
    }
    
    // 执行操作...
    co_return value * 2;
}
```

### 获取统计信息

```cpp
auto stats = server.get_stats();
std::cout << "Total requests: " << stats.total_requests << std::endl;
std::cout << "Success rate: " 
          << (stats.successful_requests * 100.0 / stats.total_requests) 
          << "%" << std::endl;
std::cout << "Average latency: " << stats.avg_latency_ms << "ms" << std::endl;
std::cout << "QPS: " << stats.qps << std::endl;
```

## RPC 客户端

### 基本用法

```cpp
#include <frpc/rpc_client.h>

RpcTask<void> example() {
    ClientConfig config;
    RpcClient client(config);
    
    // 调用远程服务
    auto result = co_await client.call<std::string>("echo", "Hello, FRPC!");
    
    if (result) {
        std::cout << "Response: " << *result << std::endl;
    }
}
```

### 超时控制

```cpp
// 设置默认超时
ClientConfig config;
config.default_timeout = std::chrono::milliseconds(3000);
RpcClient client(config);

// 为单个调用设置超时
auto result = co_await client.call<int>(
    "slow_service", 
    arg1, arg2,
    std::chrono::milliseconds(10000)  // 10秒超时
);
```

### 重试机制

```cpp
ClientConfig config;
config.max_retries = 3;  // 最多重试3次
RpcClient client(config);

// 调用会自动重试
auto result = co_await client.call<int>("unreliable_service", 42);
```

### 错误处理

```cpp
try {
    auto result = co_await client.call<int>("add", 10, 20);
    std::cout << "Result: " << *result << std::endl;
} catch (const TimeoutException& e) {
    // 处理超时
    std::cerr << "Request timeout" << std::endl;
} catch (const NetworkException& e) {
    // 处理网络错误
    std::cerr << "Network error: " << e.what() << std::endl;
} catch (const ServiceException& e) {
    // 处理服务错误
    std::cerr << "Service error: " << e.what() << std::endl;
}
```

### 获取统计信息

```cpp
auto stats = client.get_stats();
std::cout << "Total calls: " << stats.total_calls << std::endl;
std::cout << "Success rate: " 
          << (stats.successful_calls * 100.0 / stats.total_calls) 
          << "%" << std::endl;
std::cout << "Timeout rate: " 
          << (stats.timeout_calls * 100.0 / stats.total_calls) 
          << "%" << std::endl;
```

## 服务治理

### 服务注册与发现

```cpp
#include <frpc/service_registry.h>

// 创建服务注册中心
ServiceRegistry registry;

// 服务端注册服务
ServiceInstance instance{"192.168.1.100", 8080, 100};
registry.register_service("user_service", instance);

// 客户端查询服务
auto instances = registry.get_instances("user_service");
for (const auto& inst : instances) {
    std::cout << "Instance: " << inst.host << ":" << inst.port << std::endl;
}

// 订阅服务变更
registry.subscribe("user_service", [](const auto& instances) {
    std::cout << "Service instances updated, count: " 
              << instances.size() << std::endl;
});

// 注销服务
registry.unregister_service("user_service", instance);
```

### 负载均衡

#### 轮询策略

```cpp
#include <frpc/load_balancer.h>

auto lb = std::make_unique<RoundRobinLoadBalancer>();
auto instance = lb->select(instances);
```

#### 随机策略

```cpp
auto lb = std::make_unique<RandomLoadBalancer>();
auto instance = lb->select(instances);
```

#### 最少连接策略

```cpp
auto lb = std::make_unique<LeastConnectionLoadBalancer>(&connection_pool);
auto instance = lb->select(instances);
```

#### 加权轮询策略

```cpp
// 设置实例权重
ServiceInstance inst1{"192.168.1.100", 8080, 100};  // 权重 100
ServiceInstance inst2{"192.168.1.101", 8080, 200};  // 权重 200

auto lb = std::make_unique<WeightedRoundRobinLoadBalancer>();
auto instance = lb->select({inst1, inst2});
// inst2 被选中的概率是 inst1 的两倍
```

#### 自定义负载均衡

```cpp
class MyLoadBalancer : public LoadBalancer {
public:
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override {
        // 实现自定义选择逻辑
        // 例如：基于响应时间、CPU 负载等
        return instances[0];
    }
};
```

### 健康检测

```cpp
#include <frpc/health_checker.h>

// 配置健康检测
HealthCheckConfig config;
config.interval = std::chrono::seconds(10);      // 每10秒检测一次
config.timeout = std::chrono::seconds(3);        // 检测超时3秒
config.failure_threshold = 3;                    // 连续失败3次标记为不健康
config.success_threshold = 2;                    // 连续成功2次标记为健康

// 创建健康检测器
HealthChecker checker(config, &registry);

// 添加检测目标
checker.add_target("user_service", instance);

// 启动健康检测
checker.start();

// 停止健康检测
checker.stop();
```

### 连接池

```cpp
#include <frpc/connection_pool.h>

// 配置连接池
PoolConfig config;
config.min_connections = 5;                      // 最小连接数
config.max_connections = 100;                    // 最大连接数
config.idle_timeout = std::chrono::seconds(60);  // 空闲超时

// 创建连接池
ConnectionPool pool(config);

// 获取连接（协程）
auto conn = co_await pool.get_connection(instance);

// 使用连接
auto result = co_await conn.send(data);
auto response = co_await conn.recv();

// 归还连接
pool.return_connection(std::move(conn));

// 获取统计信息
auto stats = pool.get_stats();
std::cout << "Total connections: " << stats.total_connections << std::endl;
std::cout << "Idle connections: " << stats.idle_connections << std::endl;
std::cout << "Reuse rate: " << stats.connection_reuse_rate << std::endl;

// 清理空闲连接
pool.cleanup_idle_connections();
```

## 协程编程

### 协程基础

```cpp
// 定义协程函数
RpcTask<int> my_coroutine() {
    // 使用 co_await 等待异步操作
    auto result = co_await async_operation();
    
    // 使用 co_return 返回结果
    co_return result;
}

// 调用协程
auto task = my_coroutine();
// task 是一个 RpcTask<int> 对象
```

### 协程组合

```cpp
RpcTask<int> fetch_data(int id) {
    auto conn = co_await pool.get_connection(instance);
    auto data = co_await conn.recv();
    pool.return_connection(std::move(conn));
    co_return data;
}

RpcTask<void> process_multiple() {
    // 顺序执行
    auto data1 = co_await fetch_data(1);
    auto data2 = co_await fetch_data(2);
    
    // 处理数据...
}
```

### 错误处理

```cpp
RpcTask<int> safe_operation() {
    try {
        auto result = co_await risky_operation();
        co_return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Operation failed: {}", e.what());
        co_return -1;
    }
}
```

### 协程生命周期

```cpp
// 协程状态
enum class CoroutineState {
    Created,    // 已创建
    Running,    // 运行中
    Suspended,  // 已挂起
    Completed,  // 已完成
    Failed      // 失败
};

// 查询协程状态
auto state = scheduler.get_state(handle);
```

## 错误处理

### 错误码体系

```cpp
enum class ErrorCode : uint32_t {
    Success = 0,
    
    // 网络错误 (1000-1999)
    NetworkError = 1000,
    ConnectionFailed = 1001,
    Timeout = 1005,
    
    // 序列化错误 (2000-2999)
    SerializationError = 2000,
    DeserializationError = 2001,
    
    // 服务错误 (3000-3999)
    ServiceNotFound = 3000,
    ServiceUnavailable = 3001,
    NoInstanceAvailable = 3002,
    
    // 系统错误 (5000-5999)
    InternalError = 5000,
    OutOfMemory = 5001,
};

// 获取错误描述
std::string_view msg = get_error_message(ErrorCode::Timeout);
```

### 异常类型

```cpp
// 网络异常
try {
    auto conn = co_await pool.get_connection(instance);
} catch (const NetworkException& e) {
    std::cerr << "Network error: " << e.what() << std::endl;
    std::cerr << "Error code: " << static_cast<int>(e.code()) << std::endl;
}

// 序列化异常
try {
    auto data = serializer.serialize(request);
} catch (const SerializationException& e) {
    std::cerr << "Serialization error: " << e.what() << std::endl;
}

// 服务异常
try {
    auto result = co_await client.call<int>("service", args);
} catch (const ServiceException& e) {
    std::cerr << "Service error: " << e.what() << std::endl;
}

// 超时异常
try {
    auto result = co_await client.call<int>("slow_service", args);
} catch (const TimeoutException& e) {
    std::cerr << "Request timeout" << std::endl;
}
```

### 日志系统

```cpp
#include <frpc/logger.h>

// 设置日志级别
Logger::set_level(LogLevel::DEBUG);  // DEBUG, INFO, WARN, ERROR

// 设置日志输出
Logger::set_output_file("frpc.log");
Logger::set_output_console(true);

// 记录日志
LOG_DEBUG("Debug message: {}", value);
LOG_INFO("Server started on port {}", port);
LOG_WARN("Connection pool nearly full: {}/{}", active, max);
LOG_ERROR("Failed to connect to {}: {}", host, error);

// 日志格式
// [2024-01-15 10:30:45.123] [INFO] [thread-1] [coroutine-42] [network_engine] Server started on port 8080
```

## 性能优化

### 内存池

```cpp
// 协程帧使用内存池分配
// 自动优化，无需手动配置

// 查看内存池统计
auto stats = memory_pool.get_stats();
std::cout << "Total blocks: " << stats.total_blocks << std::endl;
std::cout << "Free blocks: " << stats.free_blocks << std::endl;
std::cout << "Allocations: " << stats.allocations << std::endl;
```

### 缓冲区池

```cpp
// 网络 I/O 使用缓冲区池
// 自动优化，无需手动配置

// 自定义缓冲区大小
BufferPool pool(8192, 100);  // 8KB 缓冲区，初始100个
```

### 连接复用

```cpp
// 使用连接池自动复用连接
PoolConfig config;
config.min_connections = 10;   // 保持最少10个连接
config.max_connections = 100;  // 最多100个连接
config.idle_timeout = std::chrono::seconds(60);

ConnectionPool pool(config);
```

### 批量操作

```cpp
// 批量发送请求
RpcTask<void> batch_requests() {
    std::vector<RpcTask<int>> tasks;
    
    for (int i = 0; i < 100; ++i) {
        tasks.push_back(client.call<int>("service", i));
    }
    
    // 等待所有请求完成
    for (auto& task : tasks) {
        auto result = co_await task;
        // 处理结果...
    }
}
```

## 线程模型

### 单线程模式

```cpp
ServerConfig config;
config.worker_threads = 0;  // 单线程模式（默认）
RpcServer server(config);
```

### 多线程模式

```cpp
ServerConfig config;
config.worker_threads = 4;  // 4个工作线程
RpcServer server(config);
```

### 线程安全

以下组件是线程安全的：
- `ConnectionPool`
- `ServiceRegistry`
- `CoroutineScheduler`
- `Logger`

以下组件不是线程安全的：
- `NetworkEngine`（应在单个线程中使用）
- `RpcServer`（内部管理线程安全）
- `RpcClient`（内部管理线程安全）

## 扩展开发

### 自定义序列化器

```cpp
class MySerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const Request& request) override {
        // 实现自定义序列化逻辑
        // 例如：使用 JSON、Protobuf、MessagePack 等
        return data;
    }
    
    Request deserialize_request(std::span<const uint8_t> data) override {
        // 实现自定义反序列化逻辑
        return request;
    }
    
    // 实现其他方法...
};

// 使用自定义序列化器
auto serializer = std::make_unique<MySerializer>();
```

### 自定义服务发现

```cpp
class ConsulServiceRegistry : public ServiceRegistry {
public:
    bool register_service(const std::string& name, 
                         const ServiceInstance& instance) override {
        // 向 Consul 注册服务
        return true;
    }
    
    std::vector<ServiceInstance> get_instances(
        const std::string& name) const override {
        // 从 Consul 查询服务实例
        return instances;
    }
    
    // 实现其他方法...
};
```

### 插件开发

```cpp
// 定义插件接口
class RpcPlugin {
public:
    virtual ~RpcPlugin() = default;
    virtual void on_request(const Request& req) = 0;
    virtual void on_response(const Response& resp) = 0;
};

// 实现插件
class TracingPlugin : public RpcPlugin {
public:
    void on_request(const Request& req) override {
        // 记录请求追踪信息
        trace_id = req.metadata["trace_id"];
    }
    
    void on_response(const Response& resp) override {
        // 记录响应追踪信息
    }
};
```

## 下一步

- 查看[配置说明](05-configuration.md)了解所有配置选项
- 阅读[最佳实践](07-best-practices.md)优化您的应用
- 参考[故障排查指南](08-troubleshooting.md)解决常见问题
- 查看 `examples/` 目录中的完整示例
