# 快速开始

本指南将帮助您在 5 分钟内开始使用 FRPC 框架。

## 安装

### 1. 克隆仓库

```bash
git clone https://github.com/your-org/frpc-framework.git
cd frpc-framework
```

### 2. 构建项目

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. 运行测试（可选）

```bash
ctest --output-on-failure
```

## 第一个 RPC 服务

### 步骤 1: 创建服务端

创建文件 `my_server.cpp`:

```cpp
#include <frpc/rpc_server.h>
#include <frpc/config.h>
#include <iostream>

using namespace frpc;

// 定义一个简单的加法服务
RpcTask<int> add_service(int a, int b) {
    co_return a + b;
}

int main() {
    // 加载配置
    ServerConfig config;
    config.listen_addr = "0.0.0.0";
    config.listen_port = 8080;
    config.max_connections = 1000;
    
    // 创建服务器
    RpcServer server(config);
    
    // 注册服务
    server.register_service("add", add_service);
    
    std::cout << "Server listening on " << config.listen_addr 
              << ":" << config.listen_port << std::endl;
    
    // 启动服务器
    server.start();
    
    return 0;
}
```

### 步骤 2: 创建客户端

创建文件 `my_client.cpp`:

```cpp
#include <frpc/rpc_client.h>
#include <frpc/config.h>
#include <iostream>

using namespace frpc;

RpcTask<void> call_service() {
    // 配置客户端
    ClientConfig config;
    config.default_timeout = std::chrono::milliseconds(5000);
    
    RpcClient client(config);
    
    try {
        // 调用远程服务
        auto result = co_await client.call<int>("add", 10, 20);
        
        if (result) {
            std::cout << "Result: " << *result << std::endl;
        } else {
            std::cerr << "Call failed" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    // 运行客户端协程
    auto task = call_service();
    // 等待完成...
    
    return 0;
}
```

### 步骤 3: 编译和运行

```bash
# 编译服务端
g++ -std=c++20 my_server.cpp -o my_server -lfrpc -lpthread

# 编译客户端
g++ -std=c++20 my_client.cpp -o my_client -lfrpc -lpthread

# 运行服务端
./my_server

# 在另一个终端运行客户端
./my_client
```

## 使用配置文件

### 创建服务端配置文件 `server_config.json`

```json
{
    "listen_addr": "0.0.0.0",
    "listen_port": 8080,
    "max_connections": 10000,
    "idle_timeout_seconds": 300,
    "worker_threads": 4
}
```

### 从配置文件加载

```cpp
#include <frpc/config.h>

// 加载配置
auto config = ServerConfig::load_from_file("server_config.json");

// 创建服务器
RpcServer server(config);
```

## 更多示例

### 复杂类型服务

```cpp
struct User {
    std::string name;
    int age;
    std::string email;
};

RpcTask<User> get_user(int user_id) {
    // 查询数据库...
    User user{"Alice", 30, "alice@example.com"};
    co_return user;
}

// 注册服务
server.register_service("get_user", get_user);
```

### 错误处理

```cpp
try {
    auto result = co_await client.call<int>("add", 10, 20);
    // 处理结果...
} catch (const TimeoutException& e) {
    std::cerr << "Request timeout: " << e.what() << std::endl;
} catch (const NetworkException& e) {
    std::cerr << "Network error: " << e.what() << std::endl;
} catch (const ServiceException& e) {
    std::cerr << "Service error: " << e.what() << std::endl;
}
```

### 日志配置

```cpp
#include <frpc/logger.h>

// 配置日志级别
Logger::set_level(LogLevel::INFO);

// 配置日志输出
Logger::set_output_file("frpc.log");

// 记录日志
LOG_INFO("Server started on port 8080");
LOG_ERROR("Connection failed: {}", error_message);
```

## 运行示例程序

框架提供了多个示例程序：

```bash
cd build/examples

# 运行 Hello World 示例
./hello_frpc

# 运行完整的 RPC 示例
./complete_rpc_example

# 运行计算器服务示例
./calculator_service_example

# 运行回声服务示例
./echo_service_example
```

## 常见问题

### Q: 如何处理复杂的数据类型？

A: 使用序列化器将复杂类型转换为字节流。参见[序列化格式文档](06-serialization.md)。

### Q: 如何实现自定义负载均衡策略？

A: 继承 `LoadBalancer` 基类并实现 `select()` 方法。参见[服务治理](04-user-guide.md#服务治理)。

### Q: 框架是否支持多线程？

A: 核心组件（Connection Pool、Service Registry、Coroutine Scheduler）是线程安全的。参见[线程模型](04-user-guide.md#线程模型)。

### Q: 如何监控服务性能？

A: 使用 `get_stats()` 方法获取统计信息。参见[性能监控](04-user-guide.md#性能监控)。

## 下一步

- 阅读[架构设计](03-architecture.md)了解框架内部结构
- 查看[使用指南](04-user-guide.md)了解更多功能
- 参考[配置说明](05-configuration.md)了解所有配置选项
- 学习[最佳实践](07-best-practices.md)优化您的应用
- 查看 `examples/` 目录中的完整示例

## 获取帮助

- 查看[故障排查指南](08-troubleshooting.md)
- 提交 Issue: https://github.com/your-org/frpc-framework/issues
- 查看 API 文档: `docs/html/index.html`
