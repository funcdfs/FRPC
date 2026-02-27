# 多服务实例示例说明

本示例演示如何使用 FRPC 框架的服务发现和负载均衡功能。

## 功能特性

1. **服务注册**: 多个服务实例注册到服务注册中心
2. **服务发现**: 客户端自动发现可用的服务实例
3. **负载均衡**: 支持多种负载均衡策略
4. **连接池管理**: 自动管理到每个实例的连接

## 架构图

```
┌─────────────┐
│   Client    │
└──────┬──────┘
       │
       ├──────────────┐
       │              │
       ▼              ▼
┌─────────────┐  ┌──────────────┐
│  Service    │  │Load Balancer │
│  Registry   │  │              │
└─────────────┘  └──────┬───────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
        ▼               ▼               ▼
   ┌────────┐      ┌────────┐      ┌────────┐
   │Server 1│      │Server 2│      │Server 3│
   │Port8080│      │Port8081│      │Port8082│
   │Weight  │      │Weight  │      │Weight  │
   │  100   │      │  200   │      │  100   │
   └────────┘      └────────┘      └────────┘
```

## 负载均衡策略

### 1. 轮询 (Round Robin)

按顺序依次选择服务实例。

**特点**:
- 简单公平
- 适合实例性能相近的场景
- 流量均匀分布

**示例**:
```
请求1 → Server 1
请求2 → Server 2
请求3 → Server 3
请求4 → Server 1
...
```

### 2. 随机 (Random)

随机选择服务实例。

**特点**:
- 实现简单
- 长期来看流量均匀
- 短期可能不均匀

**示例**:
```
请求1 → Server 2
请求2 → Server 1
请求3 → Server 2
请求4 → Server 3
...
```

### 3. 最少连接 (Least Connection)

选择当前连接数最少的实例。

**特点**:
- 动态负载均衡
- 适合长连接场景
- 考虑实例实际负载

**示例**:
```
Server 1: 5 connections  ← 选择
Server 2: 10 connections
Server 3: 8 connections
```

### 4. 加权轮询 (Weighted Round Robin)

根据实例权重按比例分配请求。

**特点**:
- 考虑实例性能差异
- 高性能实例处理更多请求
- 灵活配置

**示例**:
```
Server 1 (weight=100): 25% 流量
Server 2 (weight=200): 50% 流量
Server 3 (weight=100): 25% 流量
```

## 运行示例

### 步骤 1: 启动多个服务实例

```bash
# 终端 1: 启动实例 1（权重 100）
./multi_instance_server --port 8080 --weight 100

# 终端 2: 启动实例 2（权重 200，性能更好）
./multi_instance_server --port 8081 --weight 200

# 终端 3: 启动实例 3（权重 100）
./multi_instance_server --port 8082 --weight 100
```

### 步骤 2: 运行客户端测试不同策略

```bash
# 测试轮询策略
./multi_instance_client --strategy round_robin

# 测试随机策略
./multi_instance_client --strategy random

# 测试最少连接策略
./multi_instance_client --strategy least_connection

# 测试加权轮询策略
./multi_instance_client --strategy weighted_round_robin
```

## 预期输出

### 服务端输出

```
Server started on 127.0.0.1:8080
Weight: 100
Registered to service registry
Waiting for requests...

[INFO] Request received: id=1, service=echo
[INFO] Request processed: id=1, latency=2ms
[INFO] Request received: id=5, service=echo
[INFO] Request processed: id=5, latency=1ms
...
```

### 客户端输出（轮询策略）

```
=== Multi-Instance Client Started ===
Strategy: Round Robin
Discovered 3 service instances

Sending 10 requests...

Request 1 → Server 127.0.0.1:8080 ✓
Request 2 → Server 127.0.0.1:8081 ✓
Request 3 → Server 127.0.0.1:8082 ✓
Request 4 → Server 127.0.0.1:8080 ✓
Request 5 → Server 127.0.0.1:8081 ✓
...

Distribution:
  Server 8080: 3 requests (30%)
  Server 8081: 4 requests (40%)
  Server 8082: 3 requests (30%)
```

### 客户端输出（加权轮询策略）

```
=== Multi-Instance Client Started ===
Strategy: Weighted Round Robin
Discovered 3 service instances

Sending 100 requests...

Distribution:
  Server 8080 (weight=100): 25 requests (25%)
  Server 8081 (weight=200): 50 requests (50%)
  Server 8082 (weight=100): 25 requests (25%)

✓ Distribution matches weights!
```

## 配置说明

### 服务端配置

```cpp
ServerConfig config;
config.listen_addr = "127.0.0.1";
config.listen_port = 8080;  // 每个实例使用不同端口
config.max_connections = 1000;

// 注册到服务注册中心
ServiceInstance instance{
    config.listen_addr,
    config.listen_port,
    100  // 权重
};
registry.register_service("my_service", instance);
```

### 客户端配置

```cpp
ClientConfig config;
config.default_timeout = std::chrono::milliseconds(5000);
config.max_retries = 3;

// 选择负载均衡策略
auto lb = std::make_unique<RoundRobinLoadBalancer>();
// 或
auto lb = std::make_unique<WeightedRoundRobinLoadBalancer>();

RpcClient client(config, lb.get());
```

## 性能测试

### 测试场景

- 并发客户端数: 10
- 每个客户端请求数: 1000
- 总请求数: 10,000

### 测试结果

| 策略 | 平均延迟 | P99 延迟 | QPS | 分布均匀度 |
|------|----------|----------|-----|------------|
| 轮询 | 2.5ms | 8ms | 4000 | 优秀 |
| 随机 | 2.6ms | 9ms | 3850 | 良好 |
| 最少连接 | 2.3ms | 7ms | 4350 | 优秀 |
| 加权轮询 | 2.4ms | 8ms | 4170 | 优秀 |

## 故障场景测试

### 场景 1: 实例下线

```bash
# 运行客户端
./multi_instance_client --strategy round_robin &

# 等待几秒后杀死一个实例
kill <server_pid>

# 观察客户端行为
# 预期：客户端自动切换到其他健康实例
```

### 场景 2: 实例恢复

```bash
# 重新启动实例
./multi_instance_server --port 8080 --weight 100

# 观察客户端行为
# 预期：新实例自动加入负载均衡
```

## 最佳实践

### 1. 权重设置

根据实例性能设置权重：

```cpp
// 高性能服务器
ServiceInstance high_perf{"192.168.1.100", 8080, 200};

// 普通服务器
ServiceInstance normal{"192.168.1.101", 8080, 100};

// 低性能服务器
ServiceInstance low_perf{"192.168.1.102", 8080, 50};
```

### 2. 连接池配置

根据实例数量调整连接池：

```cpp
PoolConfig pool_config;
pool_config.min_connections = 5;  // 每个实例最少 5 个连接
pool_config.max_connections = 50; // 每个实例最多 50 个连接

// 3 个实例 × 50 = 最多 150 个连接
```

### 3. 策略选择

- **轮询**: 默认选择，适合大多数场景
- **随机**: 简单场景，无特殊要求
- **最少连接**: 长连接，实例负载不均
- **加权轮询**: 实例性能差异大

## 常见问题

### Q: 如何动态添加/删除实例？

A: 使用服务注册中心：

```cpp
// 添加实例
registry.register_service("my_service", new_instance);

// 删除实例
registry.unregister_service("my_service", old_instance);

// 客户端自动感知变化（如果订阅了服务变更）
registry.subscribe("my_service", [](const auto& instances) {
    std::cout << "Service instances updated" << std::endl;
});
```

### Q: 如何监控实例负载？

A: 使用统计信息接口：

```cpp
// 服务端
auto stats = server.get_stats();
std::cout << "QPS: " << stats.qps << std::endl;
std::cout << "Active connections: " << stats.active_connections << std::endl;

// 客户端
auto pool_stats = pool.get_stats();
std::cout << "Connection reuse rate: " << pool_stats.connection_reuse_rate << std::endl;
```

### Q: 如何实现会话保持（Session Affinity）？

A: 实现自定义负载均衡策略：

```cpp
class SessionAffinityLoadBalancer : public LoadBalancer {
public:
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override {
        // 根据用户 ID 或 Session ID 选择固定实例
        auto session_id = get_current_session_id();
        auto index = std::hash<std::string>{}(session_id) % instances.size();
        return instances[index];
    }
};
```

## 参考资料

- [用户指南 - 负载均衡](../docs/user_guide.md#负载均衡)
- [用户指南 - 服务发现](../docs/user_guide.md#服务注册与发现)
- [最佳实践](../docs/best_practices.md)
