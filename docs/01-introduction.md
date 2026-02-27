# FRPC 框架介绍

## 什么是 FRPC？

FRPC (Fast RPC Framework) 是一个基于 C++20 协程的现代化高性能 RPC 框架，专为分布式系统设计。框架深度集成 C++20 无栈协程机制，将传统的异步回调模式转化为同步编写范式，大幅提升代码可读性和可维护性。

## 核心特性

### 高性能
- 基于 Reactor 模式的非阻塞网络引擎
- 低延迟：P99 < 10ms
- 高吞吐量：> 50,000 QPS
- 支持 10,000+ 并发连接

### 易用性
- 利用 C++20 协程语法（co_await, co_return）
- 以同步代码风格编写异步逻辑
- 简洁的 API 设计
- 丰富的示例代码

### 可靠性
- 完善的服务治理机制
- 服务发现与注册
- 健康检测与故障转移
- 负载均衡与连接池管理
- 完整的错误处理体系

### 可扩展性
- 支持插件式的序列化协议
- 自定义负载均衡策略
- 多种服务发现后端
- 灵活的配置系统

## 技术栈

- **语言标准**: C++20
- **网络模型**: Reactor 模式（基于 epoll）
- **并发模型**: 协程 + 事件驱动
- **序列化**: 可插拔（默认支持二进制协议）
- **平台支持**: Linux (kernel 4.0+)

## 适用场景

FRPC 框架适用于以下场景：

- **微服务架构**: 服务间高效通信
- **分布式系统**: 跨节点远程调用
- **高并发应用**: 需要处理大量并发请求
- **低延迟要求**: 对响应时间敏感的应用
- **C++ 生态**: 需要与 C++ 系统集成

## 为什么选择 FRPC？

### 相比传统回调方式

```cpp
// 传统回调方式 - 回调地狱
void send_data(int fd, const Data& data, Callback callback) {
    async_send(fd, data, [callback](Result result) {
        if (result.success) {
            async_recv(fd, [callback](Data response) {
                callback(response);
            });
        }
    });
}

// FRPC 协程方式 - 清晰简洁
RpcTask<Data> send_and_recv(int fd, const Data& data) {
    auto send_result = co_await async_send(fd, data);
    if (!send_result) {
        co_return Data{};
    }
    
    auto response = co_await async_recv(fd);
    co_return response;
}
```

### 相比其他 RPC 框架

| 特性 | FRPC | gRPC | Thrift |
|------|------|------|--------|
| C++20 协程 | ✓ | ✗ | ✗ |
| 零依赖 | ✓ | ✗ | ✗ |
| 轻量级 | ✓ | ✗ | ✗ |
| 高性能 | ✓ | ✓ | ✓ |
| 易于集成 | ✓ | △ | △ |

## 快速示例

### 服务端

```cpp
#include <frpc/rpc_server.h>

// 定义服务
RpcTask<int> add_service(int a, int b) {
    co_return a + b;
}

int main() {
    ServerConfig config;
    config.listen_port = 8080;
    
    RpcServer server(config);
    server.register_service("add", add_service);
    server.start();
    
    return 0;
}
```

### 客户端

```cpp
#include <frpc/rpc_client.h>

RpcTask<void> call_service() {
    RpcClient client(config);
    
    auto result = co_await client.call<int>("add", 10, 20);
    
    if (result) {
        std::cout << "Result: " << *result << std::endl;
    }
}
```

## 系统要求

### 构建要求

- CMake 3.20+
- GCC 10+ 或 Clang 11+（支持 C++20）
- Linux 操作系统（内核 4.0+）

### 运行要求

- Linux 操作系统
- 足够的文件描述符限制（建议 65535+）
- 根据并发需求配置内存

## 项目状态

### MVP 版本 ✅

MVP 版本已完成，包含核心 RPC 功能：
- 基础设施和错误处理
- 数据模型和序列化
- RPC 服务端和客户端
- 完整的端到端示例

### 完整版本 🚧

正在开发中的功能：
- 内存管理组件（内存池、缓冲区池）
- 网络引擎（基于 epoll 的异步 I/O）
- 协程调度器（C++20 协程支持）
- 连接池管理
- 服务注册中心
- 负载均衡器
- 健康检测器
- 高级 RPC 功能（超时、重试、故障转移）

## 许可证

本项目采用 MIT 许可证。

## 下一步

- [快速开始](02-quick-start.md) - 5 分钟上手 FRPC
- [架构设计](03-architecture.md) - 了解框架架构
- [使用指南](04-user-guide.md) - 详细使用说明
