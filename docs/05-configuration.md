# FRPC 框架配置参数说明

本文档详细说明 FRPC 框架的所有配置参数。

## 配置文件格式

FRPC 支持 JSON 格式的配置文件。

## 服务端配置 (ServerConfig)

### 基本配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `listen_addr` | string | "0.0.0.0" | 监听地址，"0.0.0.0" 表示监听所有网卡 |
| `listen_port` | uint16_t | 8080 | 监听端口号 |
| `max_connections` | size_t | 10000 | 最大并发连接数 |
| `idle_timeout_seconds` | int | 300 | 连接空闲超时时间（秒） |
| `worker_threads` | size_t | 1 | 工作线程数，0 表示单线程模式 |

### 示例配置文件

```json
{
    "listen_addr": "0.0.0.0",
    "listen_port": 8080,
    "max_connections": 10000,
    "idle_timeout_seconds": 300,
    "worker_threads": 4
}
```

### 参数详解

#### listen_addr

**说明**: 服务器监听的 IP 地址。

**推荐值**:
- `"0.0.0.0"`: 监听所有网卡（生产环境推荐）
- `"127.0.0.1"`: 仅监听本地回环（开发测试）
- 特定 IP: 仅监听指定网卡

**注意事项**:
- 使用 "0.0.0.0" 时确保配置防火墙规则
- 生产环境建议使用特定 IP 提高安全性

#### listen_port

**说明**: 服务器监听的端口号。

**推荐值**:
- 开发环境: 8080, 8000, 3000
- 生产环境: 使用 1024 以上的端口
- 避免使用系统保留端口（0-1023）

**注意事项**:
- 确保端口未被其他程序占用
- Linux 下使用 1024 以下端口需要 root 权限

#### max_connections

**说明**: 服务器允许的最大并发连接数。

**推荐值**:
- 小型应用: 1000-5000
- 中型应用: 5000-10000
- 大型应用: 10000-50000

**注意事项**:
- 需要配合系统 ulimit 设置
- 每个连接消耗一定内存，根据可用内存调整
- 建议设置为预期峰值的 1.5-2 倍

**系统配置**:
```bash
# 查看当前限制
ulimit -n

# 临时设置（当前会话）
ulimit -n 65535

# 永久设置（编辑 /etc/security/limits.conf）
* soft nofile 65535
* hard nofile 65535
```

#### idle_timeout_seconds

**说明**: 连接空闲超时时间，超过此时间未活动的连接将被关闭。

**推荐值**:
- 短连接场景: 60-120 秒
- 长连接场景: 300-600 秒
- 心跳保活场景: 根据心跳间隔设置

**注意事项**:
- 设置过短可能导致频繁重连
- 设置过长会占用连接资源
- 建议配合客户端心跳机制使用

#### worker_threads

**说明**: 工作线程数量，用于处理 CPU 密集型任务。

**推荐值**:
- 单线程模式: 0 或 1（适合 I/O 密集型）
- 多线程模式: CPU 核心数或 CPU 核心数 * 2

**注意事项**:
- 0 表示单线程事件循环模式
- 多线程会增加上下文切换开销
- I/O 密集型应用建议使用单线程
- CPU 密集型应用建议使用多线程

## 客户端配置 (ClientConfig)

### 基本配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `default_timeout_ms` | int | 5000 | 默认请求超时时间（毫秒） |
| `max_retries` | size_t | 3 | 最大重试次数 |
| `load_balance_strategy` | string | "round_robin" | 负载均衡策略 |

### 示例配置文件

```json
{
    "default_timeout_ms": 5000,
    "max_retries": 3,
    "load_balance_strategy": "round_robin"
}
```

### 参数详解

#### default_timeout_ms

**说明**: 默认的 RPC 调用超时时间。

**推荐值**:
- 快速服务: 1000-3000 ms
- 普通服务: 3000-5000 ms
- 慢速服务: 5000-10000 ms

**注意事项**:
- 可以为单个调用设置不同的超时时间
- 超时时间应大于服务端处理时间 + 网络延迟
- 设置过短可能导致误判超时
- 设置过长会影响用户体验

#### max_retries

**说明**: 请求失败时的最大重试次数。

**推荐值**:
- 幂等操作: 2-3 次
- 非幂等操作: 0 次（不重试）
- 关键操作: 3-5 次

**注意事项**:
- 仅在网络错误时重试
- 服务端错误不会重试
- 重试会增加延迟
- 非幂等操作慎用重试

#### load_balance_strategy

**说明**: 负载均衡策略。

**可选值**:
- `"round_robin"`: 轮询策略（默认）
- `"random"`: 随机策略
- `"least_connection"`: 最少连接策略
- `"weighted_round_robin"`: 加权轮询策略

**推荐场景**:
- 轮询: 实例性能相近，流量均匀
- 随机: 简单场景，无特殊要求
- 最少连接: 实例性能差异大，长连接场景
- 加权轮询: 实例性能差异大，需要按比例分配

## 连接池配置 (PoolConfig)

### 基本配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `min_connections` | size_t | 1 | 最小连接数 |
| `max_connections` | size_t | 100 | 最大连接数 |
| `idle_timeout_seconds` | int | 60 | 连接空闲超时（秒） |
| `cleanup_interval_seconds` | int | 30 | 清理间隔（秒） |

### 示例配置文件

```json
{
    "min_connections": 5,
    "max_connections": 100,
    "idle_timeout_seconds": 60,
    "cleanup_interval_seconds": 30
}
```

### 参数详解

#### min_connections

**说明**: 连接池维护的最小连接数。

**推荐值**:
- 低频调用: 1-5
- 中频调用: 5-10
- 高频调用: 10-20

**注意事项**:
- 预创建连接可以减少首次调用延迟
- 设置过大会浪费资源
- 根据实际 QPS 调整

#### max_connections

**说明**: 连接池允许的最大连接数。

**推荐值**:
- 小规模: 50-100
- 中规模: 100-500
- 大规模: 500-1000

**注意事项**:
- 应小于服务端 max_connections
- 考虑客户端实例数量
- 例如：10 个客户端实例，每个 max_connections=100，服务端至少需要 1000

#### idle_timeout_seconds

**说明**: 连接空闲多久后被清理。

**推荐值**:
- 短连接: 30-60 秒
- 长连接: 60-300 秒

**注意事项**:
- 应小于服务端 idle_timeout
- 设置过短会导致频繁重连
- 设置过长会占用资源

#### cleanup_interval_seconds

**说明**: 清理空闲连接的检查间隔。

**推荐值**: 30-60 秒

**注意事项**:
- 设置过短会增加 CPU 开销
- 设置过长会延迟资源释放

## 健康检测配置 (HealthCheckConfig)

### 基本配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `interval_seconds` | int | 10 | 检测间隔（秒） |
| `timeout_seconds` | int | 3 | 检测超时（秒） |
| `failure_threshold` | int | 3 | 失败阈值（次） |
| `success_threshold` | int | 2 | 成功阈值（次） |

### 示例配置文件

```json
{
    "interval_seconds": 10,
    "timeout_seconds": 3,
    "failure_threshold": 3,
    "success_threshold": 2
}
```

### 参数详解

#### interval_seconds

**说明**: 健康检测的时间间隔。

**推荐值**:
- 快速检测: 5-10 秒
- 普通检测: 10-30 秒
- 慢速检测: 30-60 秒

**注意事项**:
- 设置过短会增加服务端负载
- 设置过长会延迟故障发现
- 根据服务稳定性调整

#### timeout_seconds

**说明**: 单次健康检测的超时时间。

**推荐值**: 2-5 秒

**注意事项**:
- 应小于 interval_seconds
- 设置过短可能误判
- 设置过长会延迟故障发现

#### failure_threshold

**说明**: 连续失败多少次后标记为不健康。

**推荐值**:
- 敏感检测: 2-3 次
- 普通检测: 3-5 次
- 宽松检测: 5-10 次

**注意事项**:
- 设置过小容易误判
- 设置过大会延迟故障发现
- 考虑网络抖动因素

#### success_threshold

**说明**: 连续成功多少次后标记为健康。

**推荐值**: 1-3 次

**注意事项**:
- 设置过大会延迟恢复
- 通常小于 failure_threshold

## 日志配置

### 代码配置

```cpp
#include <frpc/logger.h>

// 设置日志级别
Logger::set_level(LogLevel::INFO);

// 设置日志输出文件
Logger::set_output_file("/var/log/frpc/app.log");

// 设置是否输出到控制台
Logger::set_output_console(true);

// 设置日志轮转（可选）
Logger::set_max_file_size(100 * 1024 * 1024);  // 100MB
Logger::set_max_files(10);  // 保留10个文件
```

### 日志级别

| 级别 | 说明 | 使用场景 |
|------|------|----------|
| DEBUG | 调试信息 | 开发调试 |
| INFO | 一般信息 | 生产环境（推荐） |
| WARN | 警告信息 | 生产环境 |
| ERROR | 错误信息 | 生产环境 |

**推荐配置**:
- 开发环境: DEBUG
- 测试环境: INFO
- 生产环境: INFO 或 WARN

## 完整配置示例

### 服务端完整配置

```json
{
    "server": {
        "listen_addr": "0.0.0.0",
        "listen_port": 8080,
        "max_connections": 10000,
        "idle_timeout_seconds": 300,
        "worker_threads": 4
    },
    "connection_pool": {
        "min_connections": 5,
        "max_connections": 100,
        "idle_timeout_seconds": 60,
        "cleanup_interval_seconds": 30
    },
    "health_check": {
        "interval_seconds": 10,
        "timeout_seconds": 3,
        "failure_threshold": 3,
        "success_threshold": 2
    },
    "logging": {
        "level": "INFO",
        "file": "/var/log/frpc/server.log",
        "console": true,
        "max_file_size_mb": 100,
        "max_files": 10
    }
}
```

### 客户端完整配置

```json
{
    "client": {
        "default_timeout_ms": 5000,
        "max_retries": 3,
        "load_balance_strategy": "round_robin"
    },
    "connection_pool": {
        "min_connections": 5,
        "max_connections": 100,
        "idle_timeout_seconds": 60,
        "cleanup_interval_seconds": 30
    },
    "logging": {
        "level": "INFO",
        "file": "/var/log/frpc/client.log",
        "console": false
    }
}
```

## 性能调优建议

### 高并发场景

```json
{
    "server": {
        "max_connections": 50000,
        "worker_threads": 8,
        "idle_timeout_seconds": 120
    },
    "connection_pool": {
        "min_connections": 20,
        "max_connections": 500
    }
}
```

### 低延迟场景

```json
{
    "client": {
        "default_timeout_ms": 1000,
        "max_retries": 1
    },
    "connection_pool": {
        "min_connections": 10,
        "max_connections": 50
    },
    "health_check": {
        "interval_seconds": 5,
        "timeout_seconds": 2
    }
}
```

### 资源受限场景

```json
{
    "server": {
        "max_connections": 1000,
        "worker_threads": 1
    },
    "connection_pool": {
        "min_connections": 1,
        "max_connections": 20,
        "idle_timeout_seconds": 30
    }
}
```

## 环境变量

支持通过环境变量覆盖配置：

```bash
export FRPC_LISTEN_PORT=9090
export FRPC_MAX_CONNECTIONS=20000
export FRPC_LOG_LEVEL=DEBUG
```

## 配置验证

使用配置验证工具检查配置文件：

```bash
./frpc_config_validator server_config.json
```

## 常见问题

### Q: 如何选择合适的 max_connections？

A: 根据以下公式估算：
```
max_connections = 预期 QPS × 平均响应时间（秒） × 安全系数（1.5-2）
```

### Q: 连接池大小如何设置？

A: 客户端连接池大小应考虑：
```
max_connections = 预期并发请求数 / 客户端实例数
```

### Q: 健康检测参数如何调整？

A: 根据服务稳定性：
- 稳定服务：interval=30s, failure_threshold=5
- 不稳定服务：interval=10s, failure_threshold=3

## 参考资料

- [快速入门](02-quick-start.md)
- [用户指南](04-user-guide.md)
- [最佳实践](07-best-practices.md)
- [故障排查](08-troubleshooting.md)
