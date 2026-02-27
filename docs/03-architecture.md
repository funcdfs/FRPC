# 架构设计

## 整体架构

FRPC 框架采用分层架构设计，从下到上分为以下几层：

```
┌─────────────────────────────────────────┐
│      应用层 (Application Layer)          │
│         用户服务实现                      │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│       RPC 层 (RPC Layer)                │
│    RPC 客户端 / RPC 服务端               │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│   服务治理层 (Service Governance)        │
│  服务发现 / 负载均衡 / 健康检测          │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│     协程层 (Coroutine Layer)            │
│      协程调度器 / 协程管理               │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│     网络层 (Network Layer)              │
│   网络引擎 / 连接池 / 序列化             │
└─────────────────────────────────────────┘
```

## 核心组件

### 1. 网络引擎 (NetworkEngine)

基于 Reactor 模式的事件驱动网络层。

**职责**:
- 管理 socket 连接
- 事件多路复用（epoll）
- 非阻塞 I/O 操作
- 事件分发和回调

**特性**:
- 支持 10,000+ 并发连接
- 零拷贝优化
- 边缘触发模式
- 自动重连机制

**关键接口**:
```cpp
class NetworkEngine {
public:
    void run();
    void stop();
    Awaitable<SendResult> async_send(int fd, std::span<const uint8_t> data);
    Awaitable<RecvResult> async_recv(int fd);
};
```

### 2. 协程调度器 (CoroutineScheduler)

管理 C++20 协程的生命周期和调度。

**职责**:
- 协程创建和销毁
- 协程挂起和恢复
- 协程优先级调度
- 协程状态管理

**特性**:
- 支持数万并发协程
- 内存池优化
- 公平调度算法
- 死锁检测

**关键接口**:
```cpp
class CoroutineScheduler {
public:
    void schedule(CoroutineHandle handle);
    void resume(CoroutineHandle handle);
    void suspend(CoroutineHandle handle);
    CoroutineState get_state(CoroutineHandle handle);
};
```

### 3. 连接池 (ConnectionPool)

管理和复用网络连接。

**职责**:
- 连接创建和销毁
- 连接复用
- 连接健康检查
- 连接超时管理

**特性**:
- 最小/最大连接数控制
- 空闲连接回收
- 连接预热
- 线程安全

**关键接口**:
```cpp
class ConnectionPool {
public:
    Awaitable<Connection> get_connection(const ServiceInstance& instance);
    void return_connection(Connection conn);
    void cleanup_idle_connections();
    PoolStats get_stats();
};
```

### 4. 服务注册中心 (ServiceRegistry)

管理服务实例的注册和发现。

**职责**:
- 服务注册和注销
- 服务查询
- 服务变更通知
- 服务元数据管理

**特性**:
- 支持多种后端（内存、Consul、etcd）
- 服务订阅机制
- 自动心跳
- 线程安全

**关键接口**:
```cpp
class ServiceRegistry {
public:
    bool register_service(const std::string& name, const ServiceInstance& instance);
    void unregister_service(const std::string& name, const ServiceInstance& instance);
    std::vector<ServiceInstance> get_instances(const std::string& name);
    void subscribe(const std::string& name, Callback callback);
};
```

### 5. 负载均衡器 (LoadBalancer)

在多个服务实例间分配请求。

**职责**:
- 实例选择
- 负载分配
- 故障转移
- 性能监控

**支持策略**:
- 轮询 (Round Robin)
- 随机 (Random)
- 最少连接 (Least Connection)
- 加权轮询 (Weighted Round Robin)
- 一致性哈希 (Consistent Hash)

**关键接口**:
```cpp
class LoadBalancer {
public:
    virtual ServiceInstance select(const std::vector<ServiceInstance>& instances) = 0;
};
```

### 6. 健康检测器 (HealthChecker)

监控服务实例的健康状态。

**职责**:
- 定期健康检查
- 故障检测
- 自动恢复检测
- 状态更新通知

**特性**:
- 可配置检查间隔
- 失败阈值控制
- 多种检查方式（TCP、HTTP、自定义）
- 异步检查

**关键接口**:
```cpp
class HealthChecker {
public:
    void start();
    void stop();
    void add_target(const std::string& service, const ServiceInstance& instance);
    void remove_target(const std::string& service, const ServiceInstance& instance);
};
```

### 7. 序列化器 (Serializer)

处理数据的序列化和反序列化。

**职责**:
- 请求序列化
- 响应反序列化
- 协议编解码
- 数据校验

**特性**:
- 紧凑的二进制格式
- 零拷贝优化
- 版本兼容
- 可扩展

**关键接口**:
```cpp
class Serializer {
public:
    std::vector<uint8_t> serialize(const Request& request);
    Request deserialize_request(std::span<const uint8_t> data);
    std::vector<uint8_t> serialize(const Response& response);
    Response deserialize_response(std::span<const uint8_t> data);
};
```

### 8. RPC 服务端 (RpcServer)

处理 RPC 请求的服务端。

**职责**:
- 服务注册
- 请求接收
- 服务路由
- 响应发送

**特性**:
- 多线程支持
- 请求并发处理
- 优雅关闭
- 统计信息

**关键接口**:
```cpp
class RpcServer {
public:
    void register_service(const std::string& name, ServiceHandler handler);
    void start();
    void stop();
    ServerStats get_stats();
};
```

### 9. RPC 客户端 (RpcClient)

发起 RPC 调用的客户端。

**职责**:
- 服务调用
- 超时控制
- 重试机制
- 故障转移

**特性**:
- 异步调用
- 批量请求
- 请求取消
- 统计信息

**关键接口**:
```cpp
class RpcClient {
public:
    template<typename R, typename... Args>
    Awaitable<std::optional<R>> call(const std::string& service, Args&&... args);
    
    ClientStats get_stats();
};
```

## RPC 调用流程

### 客户端调用流程

```
1. 用户调用 client.call()
         ↓
2. 服务发现查询可用实例
         ↓
3. 负载均衡选择实例
         ↓
4. 从连接池获取连接
         ↓
5. 序列化请求参数
         ↓
6. 通过网络引擎发送请求
         ↓
7. 协程挂起等待响应
         ↓
8. 接收响应数据
         ↓
9. 反序列化响应
         ↓
10. 归还连接到连接池
         ↓
11. 协程恢复，返回结果
```

### 服务端处理流程

```
1. 网络引擎接收请求
         ↓
2. 反序列化请求
         ↓
3. 根据服务名路由到处理函数
         ↓
4. 创建协程执行服务函数
         ↓
5. 服务函数处理业务逻辑
         ↓
6. 序列化返回值
         ↓
7. 通过网络引擎发送响应
         ↓
8. 更新统计信息
```

## 协程模型

### 协程生命周期

```
Created → Running → Suspended → Running → ... → Completed
                        ↓
                     Failed
```

### 协程状态转换

- **Created**: 协程已创建但未开始执行
- **Running**: 协程正在执行
- **Suspended**: 协程在 co_await 处挂起
- **Completed**: 协程正常完成
- **Failed**: 协程因异常失败

### 协程调度策略

1. **公平调度**: 所有协程轮流执行
2. **优先级调度**: 高优先级协程优先执行
3. **工作窃取**: 多线程间负载均衡

## 线程模型

### 单线程模式

```
┌─────────────────────────────┐
│      Main Thread            │
│  ┌─────────────────────┐    │
│  │  Event Loop         │    │
│  │  - Network I/O      │    │
│  │  - Coroutine Sched  │    │
│  │  - Service Handler  │    │
│  └─────────────────────┘    │
└─────────────────────────────┘
```

适用场景：I/O 密集型应用

### 多线程模式

```
┌─────────────────────────────┐
│      Main Thread            │
│  ┌─────────────────────┐    │
│  │  Event Loop         │    │
│  │  - Network I/O      │    │
│  └─────────────────────┘    │
└─────────────────────────────┘
         ↓ dispatch
┌─────────────────────────────┐
│    Worker Thread Pool       │
│  ┌─────────┐  ┌─────────┐  │
│  │ Worker 1│  │ Worker 2│  │
│  │ Handler │  │ Handler │  │
│  └─────────┘  └─────────┘  │
└─────────────────────────────┘
```

适用场景：CPU 密集型应用

## 内存管理

### 内存池

用于协程帧的快速分配和释放。

**特性**:
- 固定大小块分配
- 无锁设计
- 线程本地缓存
- 自动扩容

### 缓冲区池

用于网络 I/O 缓冲区的复用。

**特性**:
- 可配置缓冲区大小
- 引用计数管理
- 自动回收
- 内存限制

## 错误处理

### 错误码体系

```
0: Success
1000-1999: 网络错误
2000-2999: 序列化错误
3000-3999: 服务错误
4000-4999: 参数错误
5000-5999: 系统错误
```

### 异常层次

```
std::exception
    ↓
FrpcException
    ├── NetworkException
    ├── SerializationException
    ├── ServiceException
    ├── TimeoutException
    └── SystemException
```

## 性能优化

### 零拷贝

- 使用 `std::span` 避免数据拷贝
- 直接在网络缓冲区上序列化
- 引用计数管理共享数据

### 内存池

- 协程帧使用内存池分配
- 网络缓冲区使用缓冲区池
- 减少系统内存分配开销

### 批量操作

- 批量发送请求
- 批量处理事件
- 减少系统调用次数

### 连接复用

- 连接池管理
- 长连接保持
- 减少连接建立开销

## 可扩展性

### 插件机制

- 自定义序列化器
- 自定义负载均衡策略
- 自定义服务发现后端
- 自定义健康检查方式

### 配置系统

- JSON 配置文件
- 环境变量覆盖
- 运行时动态配置
- 配置验证

## 下一步

- [使用指南](04-user-guide.md) - 详细使用说明
- [配置说明](05-configuration.md) - 配置参数详解
- [API 参考](09-api-reference.md) - API 文档
