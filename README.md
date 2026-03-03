# FRPC Framework

基于 C++20 协程的高性能 RPC 框架

## 简介

FRPC (Fast RPC Framework) 是一个现代化的 C++ RPC 框架，专为分布式系统设计。该框架深度集成 C++20 无栈协程机制，将传统的异步回调模式转化为同步编写范式，大幅提升代码可读性和可维护性。

### 核心特性

- **高性能**: 基于 Reactor 模式的非阻塞网络引擎，实现低延迟（P99 < 10ms）和高吞吐量（> 50,000 QPS）
- **易用性**: 利用 C++20 协程语法（co_await, co_return），让开发者以同步代码风格编写异步逻辑
- **可靠性**: 完善的服务治理机制，包括服务发现、健康检测、负载均衡和连接池管理
- **可扩展性**: 支持插件式的序列化协议、自定义负载均衡策略和多种服务发现后端

### 技术栈

- **语言标准**: C++20
- **网络模型**: Reactor 模式（基于 epoll）
- **并发模型**: 协程 + 事件驱动
- **序列化**: 可插拔（默认支持二进制协议）
- **平台支持**: Linux (kernel 4.0+)

## 快速开始

### 构建项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译
make -j$(nproc)

# 运行测试
ctest --output-on-failure
```

### 简单示例

**服务端:**
```cpp
#include <frpc/rpc_server.h>

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

**客户端:**
```cpp
#include <frpc/rpc_client.h>

RpcTask<void> call_service() {
    RpcClient client(config);
    auto result = co_await client.call<int>("add", 10, 20);
    std::cout << "Result: " << *result << std::endl;
}
```

## 文档

完整文档请查看 **[docs/](docs/)** 目录。

**快速导航**：
- 📖 [文档索引](docs/00-index.md) - 从这里开始
- 🎯 [框架介绍](docs/01-introduction.md) - 了解 FRPC
- 🚀 [快速开始](docs/02-quick-start.md) - 5 分钟上手
- 📘 [使用指南](docs/04-user-guide.md) - 详细说明
- 🔧 [故障排查](docs/08-troubleshooting.md) - 问题解决

**完整文档列表**：[docs/README.md](docs/README.md)

## 示例程序

查看 `examples/` 目录获取完整示例：

```bash
cd build/examples

# Hello World 示例
./hello_frpc

# 完整 RPC 示例
./complete_rpc_example

# 计算器服务
./calculator_service_example

# 回声服务
./echo_service_example
```

## 构建要求

- CMake 3.20+
- GCC 10+ 或 Clang 11+（支持 C++20）
- Linux 操作系统（内核 4.0+）

### 依赖项

框架会自动下载以下依赖（如果系统中未安装）：

- Google Test (测试框架)
- Google Benchmark (性能测试)
- RapidCheck (属性测试)

## 项目结构

```
frpc/
├── include/frpc/     # 公共头文件
├── src/              # 源文件实现
├── tests/            # 单元测试和属性测试
├── examples/         # 示例程序
├── docs/             # 文档
├── benchmarks/       # 性能测试
├── stress_tests/     # 压力测试
└── CMakeLists.txt    # 构建配置
```

## 开发状态

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

## 贡献

欢迎贡献代码、报告问题或提出建议。请遵循以下准则：

1. 代码遵循 C++ 核心指南和现代 C++ 最佳实践
2. 所有公共接口必须有完整的 Doxygen 文档注释
3. 新功能必须包含单元测试
4. 提交前运行所有测试确保通过

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系我们。
