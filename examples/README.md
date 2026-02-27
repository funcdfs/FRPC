# FRPC Framework Examples

本目录包含 FRPC 框架的示例程序，展示框架的各种功能和使用方法。

## 示例列表

### 1. Hello FRPC (hello_frpc.cpp)

最简单的 RPC 示例，演示基本的客户端-服务端通信。

**功能**:
- 创建简单的 RPC 服务器
- 注册 echo 服务
- 客户端调用远程服务

**运行**:
```bash
# 编译
cd build
make hello_frpc

# 运行
./examples/hello_frpc
```

### 2. Echo 服务示例 (echo_service_example.cpp)

简单的 echo 服务，演示最基本的 RPC 调用。

**功能**:
- 服务端接收字符串并原样返回
- 客户端发送消息并接收响应
- 演示基本的错误处理

**运行**:
```bash
# 编译
make echo_service_example

# 启动服务端
./examples/echo_service_server

# 在另一个终端启动客户端
./examples/echo_service_client
```

### 3. 计算服务示例 (calculator_service_example.cpp)

演示参数传递和不同数据类型的处理。

**功能**:
- 加法、减法、乘法、除法服务
- 演示整数和浮点数参数传递
- 演示错误处理（除零错误）

**运行**:
```bash
# 编译
make calculator_service_example

# 启动服务端
./examples/calculator_service_server

# 在另一个终端启动客户端
./examples/calculator_service_client
```

### 4. 多服务实例示例 (multi_instance_example.cpp)

演示负载均衡和服务发现。

**功能**:
- 启动多个服务实例
- 服务注册和发现
- 不同的负载均衡策略（轮询、随机、最少连接、加权轮询）
- 连接池管理

**运行**:
```bash
# 编译
make multi_instance_example

# 启动多个服务实例
./examples/multi_instance_server --port 8080 --weight 100 &
./examples/multi_instance_server --port 8081 --weight 200 &
./examples/multi_instance_server --port 8082 --weight 100 &

# 启动客户端
./examples/multi_instance_client --strategy round_robin
```

### 5. 健康检测示例 (health_check_example.cpp)

演示健康检测和故障转移。

**功能**:
- 健康检测配置
- 自动检测不健康实例
- 故障转移到健康实例
- 实例恢复后自动重新加入

**运行**:
```bash
# 编译
make health_check_example

# 启动服务实例
./examples/health_check_server --port 8080 &
./examples/health_check_server --port 8081 &

# 启动客户端（包含健康检测）
./examples/health_check_client

# 模拟故障：杀死一个服务实例
kill <pid_of_8080>

# 观察客户端自动切换到健康实例
```

### 6. 完整 RPC 示例 (complete_rpc_example.cpp)

综合示例，展示框架的完整功能。

**功能**:
- 服务注册和发现
- 负载均衡
- 健康检测
- 连接池管理
- 错误处理和重试
- 性能监控

**运行**:
```bash
# 编译
make complete_rpc_example

# 运行
./examples/complete_rpc_example
```

### 7. 协程使用示例 (coroutine_usage_example.cpp)

演示 C++20 协程的使用方法。

**功能**:
- 协程基础语法
- co_await 异步操作
- 协程组合和嵌套
- 错误处理

**运行**:
```bash
make coroutine_usage_example
./examples/coroutine_usage_example
```

### 8. 序列化示例 (serializer_example.cpp)

演示序列化和反序列化。

**功能**:
- Request/Response 序列化
- 不同数据类型的序列化
- 序列化往返测试

**运行**:
```bash
make serializer_example
./examples/serializer_example
```

### 9. 数据模型示例 (data_models_example.cpp)

演示数据模型的使用。

**功能**:
- Request/Response 对象创建
- 元数据使用
- 错误码处理

**运行**:
```bash
make data_models_example
./examples/data_models_example
```

### 10. 配置使用示例 (config_usage_example.cpp)

演示配置文件的加载和使用。

**功能**:
- 从 JSON 文件加载配置
- 配置验证
- 默认配置

**运行**:
```bash
make config_usage_example
./examples/config_usage_example
```

### 11. 服务器统计示例 (server_stats_example.cpp)

演示性能监控和统计信息收集。

**功能**:
- 服务器统计信息
- 客户端统计信息
- 连接池统计信息
- 实时性能监控

**运行**:
```bash
make server_stats_example
./examples/server_stats_example
```

### 12. 集成示例 (integration_example.cpp)

端到端集成测试示例。

**功能**:
- 完整的客户端-服务端通信
- 多种服务类型
- 并发请求处理

**运行**:
```bash
make integration_example
./examples/integration_example
```

## 编译所有示例

```bash
cd build
cmake ..
make examples
```

## 配置文件

### 服务端配置 (server_config.json)

```json
{
    "listen_addr": "0.0.0.0",
    "listen_port": 8080,
    "max_connections": 10000,
    "idle_timeout_seconds": 300,
    "worker_threads": 4
}
```

### 客户端配置 (client_config.json)

```json
{
    "default_timeout_ms": 5000,
    "max_retries": 3,
    "load_balance_strategy": "round_robin"
}
```

## 示例说明

### 基础示例

适合初学者，了解框架基本用法：
1. Hello FRPC
2. Echo 服务示例
3. 计算服务示例

### 进阶示例

适合了解框架高级功能：
4. 多服务实例示例
5. 健康检测示例
6. 完整 RPC 示例

### 技术示例

适合深入了解框架内部机制：
7. 协程使用示例
8. 序列化示例
9. 数据模型示例
10. 配置使用示例
11. 服务器统计示例

## 学习路径

### 第一步：基础概念

1. 阅读 [快速入门指南](../docs/quick_start.md)
2. 运行 `hello_frpc` 示例
3. 运行 `echo_service_example` 示例

### 第二步：核心功能

1. 运行 `calculator_service_example` 了解参数传递
2. 运行 `coroutine_usage_example` 了解协程
3. 运行 `serializer_example` 了解序列化

### 第三步：服务治理

1. 运行 `multi_instance_example` 了解负载均衡
2. 运行 `health_check_example` 了解健康检测
3. 运行 `complete_rpc_example` 了解完整功能

### 第四步：生产实践

1. 阅读 [最佳实践](../docs/best_practices.md)
2. 阅读 [配置说明](../docs/configuration.md)
3. 参考 `server_stats_example` 实现监控

## 常见问题

### Q: 示例编译失败

A: 确保已安装所有依赖并正确配置 CMake：
```bash
mkdir build && cd build
cmake ..
make
```

### Q: 示例运行时连接失败

A: 确保服务端已启动并监听正确的端口：
```bash
# 检查端口
netstat -tlnp | grep 8080

# 检查防火墙
sudo iptables -L -n
```

### Q: 如何修改示例代码

A: 示例代码可以自由修改和扩展：
```bash
# 复制示例
cp examples/echo_service_example.cpp my_service.cpp

# 修改 CMakeLists.txt 添加新的可执行文件
add_executable(my_service my_service.cpp)
target_link_libraries(my_service frpc)
```

## 贡献示例

欢迎贡献新的示例程序！请遵循以下准则：

1. 代码清晰，注释详细
2. 包含完整的错误处理
3. 提供运行说明
4. 更新本 README 文件

## 参考资料

- [快速入门](../docs/quick_start.md)
- [用户指南](../docs/user_guide.md)
- [API 文档](../docs/html/index.html)
- [最佳实践](../docs/best_practices.md)
- [故障排查](../docs/troubleshooting.md)
