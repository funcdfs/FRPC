# FRPC 框架配置文件示例

本文档提供各种场景下的配置文件示例和详细说明。

## 目录

1. [基础配置](#基础配置)
2. [生产环境配置](#生产环境配置)
3. [开发环境配置](#开发环境配置)
4. [高并发场景配置](#高并发场景配置)
5. [低延迟场景配置](#低延迟场景配置)
6. [资源受限场景配置](#资源受限场景配置)
7. [完整配置示例](#完整配置示例)

## 基础配置

### 最小服务端配置

**文件**: `server_minimal.json`

```json
{
  "listen_addr": "127.0.0.1",
  "listen_port": 8080
}
```

**说明**:
- 仅包含必需参数
- 其他参数使用默认值
- 适合快速测试

### 最小客户端配置

**文件**: `client_minimal.json`

```json
{
  "default_timeout_ms": 5000
}
```

**说明**:
- 仅设置超时时间
- 其他参数使用默认值
- 适合简单场景

## 生产环境配置

### 生产服务端配置

**文件**: `server_production.json`

```json
{
  "server": {
    "listen_addr": "0.0.0.0",
    "listen_port": 8080,
    "max_connections": 50000,
    "idle_timeout_seconds": 300,
    "worker_threads": 8
  },
  "connection_pool": {
    "min_connections": 10,
    "max_connections": 200,
    "idle_timeout_seconds": 240,
    "cleanup_interval_seconds": 60
  },
  "health_check": {
    "interval_seconds": 15,
    "timeout_seconds": 5,
    "failure_threshold": 3,
    "success_threshold": 2
  },
  "logging": {
    "level": "INFO",
    "file": "/var/log/frpc/server.log",
    "console": false,
    "max_file_size_mb": 100,
    "max_files": 10
  }
}
```

**参数说明**:

| 参数 | 值 | 说明 |
|------|-----|------|
| listen_addr | "0.0.0.0" | 监听所有网卡 |
| max_connections | 50000 | 支持大量并发连接 |
| worker_threads | 8 | 8 个工作线程（根据 CPU 核心数调整） |
| min_connections | 10 | 预创建 10 个连接 |
| max_connections | 200 | 每个实例最多 200 个连接 |
| idle_timeout | 240s | 连接空闲 4 分钟后清理（小于服务端） |
| log_level | INFO | 生产环境日志级别 |
| log_file | /var/log/... | 日志文件路径 |

### 生产客户端配置

**文件**: `client_production.json`

```json
{
  "client": {
    "default_timeout_ms": 5000,
    "max_retries": 3,
    "load_balance_strategy": "weighted_round_robin"
  },
  "connection_pool": {
    "min_connections": 5,
    "max_connections": 100,
    "idle_timeout_seconds": 240,
    "cleanup_interval_seconds": 60
  },
  "logging": {
    "level": "INFO",
    "file": "/var/log/frpc/client.log",
    "console": false
  }
}
```

**参数说明**:

| 参数 | 值 | 说明 |
|------|-----|------|
| default_timeout | 5000ms | 默认 5 秒超时 |
| max_retries | 3 | 最多重试 3 次 |
| load_balance_strategy | weighted_round_robin | 加权轮询策略 |
| min_connections | 5 | 预创建 5 个连接 |
| max_connections | 100 | 每个实例最多 100 个连接 |

## 开发环境配置

### 开发服务端配置

**文件**: `server_development.json`

```json
{
  "server": {
    "listen_addr": "127.0.0.1",
    "listen_port": 8080,
    "max_connections": 1000,
    "idle_timeout_seconds": 60,
    "worker_threads": 1
  },
  "connection_pool": {
    "min_connections": 1,
    "max_connections": 20,
    "idle_timeout_seconds": 30,
    "cleanup_interval_seconds": 15
  },
  "health_check": {
    "interval_seconds": 5,
    "timeout_seconds": 2,
    "failure_threshold": 2,
    "success_threshold": 1
  },
  "logging": {
    "level": "DEBUG",
    "file": "logs/server_dev.log",
    "console": true,
    "max_file_size_mb": 10,
    "max_files": 3
  }
}
```

**特点**:
- 仅监听本地回环
- 较小的连接数限制
- 单线程模式
- DEBUG 日志级别
- 快速健康检测
- 日志同时输出到控制台和文件

### 开发客户端配置

**文件**: `client_development.json`

```json
{
  "client": {
    "default_timeout_ms": 10000,
    "max_retries": 1,
    "load_balance_strategy": "round_robin"
  },
  "connection_pool": {
    "min_connections": 1,
    "max_connections": 10,
    "idle_timeout_seconds": 30,
    "cleanup_interval_seconds": 15
  },
  "logging": {
    "level": "DEBUG",
    "console": true
  }
}
```

**特点**:
- 较长的超时时间（便于调试）
- 较少的重试次数
- 简单的负载均衡策略
- DEBUG 日志级别

## 高并发场景配置

### 高并发服务端配置

**文件**: `server_high_concurrency.json`

```json
{
  "server": {
    "listen_addr": "0.0.0.0",
    "listen_port": 8080,
    "max_connections": 100000,
    "idle_timeout_seconds": 120,
    "worker_threads": 16
  },
  "connection_pool": {
    "min_connections": 20,
    "max_connections": 500,
    "idle_timeout_seconds": 100,
    "cleanup_interval_seconds": 30
  },
  "health_check": {
    "interval_seconds": 10,
    "timeout_seconds": 3,
    "failure_threshold": 3,
    "success_threshold": 2
  },
  "logging": {
    "level": "WARN",
    "file": "/var/log/frpc/server.log",
    "console": false,
    "max_file_size_mb": 200,
    "max_files": 20
  },
  "performance": {
    "buffer_pool_size": 10000,
    "memory_pool_size": 50000,
    "enable_zero_copy": true
  }
}
```

**优化要点**:
- 超大连接数限制（100,000）
- 多工作线程（16 个）
- 大连接池（500 个/实例）
- 较短的空闲超时（快速释放资源）
- WARN 日志级别（减少 I/O）
- 大缓冲区池和内存池
- 启用零拷贝优化

### 系统配置

```bash
# /etc/security/limits.conf
* soft nofile 200000
* hard nofile 200000

# /etc/sysctl.conf
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.ip_local_port_range = 1024 65535
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 30
```

## 低延迟场景配置

### 低延迟服务端配置

**文件**: `server_low_latency.json`

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
    "min_connections": 20,
    "max_connections": 100,
    "idle_timeout_seconds": 240,
    "cleanup_interval_seconds": 60
  },
  "health_check": {
    "interval_seconds": 5,
    "timeout_seconds": 2,
    "failure_threshold": 2,
    "success_threshold": 1
  },
  "performance": {
    "enable_tcp_nodelay": true,
    "enable_tcp_quickack": true,
    "send_buffer_size": 65536,
    "recv_buffer_size": 65536,
    "enable_zero_copy": true
  },
  "logging": {
    "level": "ERROR",
    "file": "/var/log/frpc/server.log",
    "console": false,
    "async_logging": true
  }
}
```

**优化要点**:
- 预创建大量连接（减少握手延迟）
- 启用 TCP_NODELAY（禁用 Nagle 算法）
- 启用 TCP_QUICKACK（快速确认）
- 大发送/接收缓冲区
- 快速健康检测
- ERROR 日志级别（最小化日志开销）
- 异步日志（不阻塞主线程）

### 低延迟客户端配置

**文件**: `client_low_latency.json`

```json
{
  "client": {
    "default_timeout_ms": 1000,
    "max_retries": 1,
    "load_balance_strategy": "least_connection"
  },
  "connection_pool": {
    "min_connections": 10,
    "max_connections": 50,
    "idle_timeout_seconds": 240,
    "cleanup_interval_seconds": 60
  },
  "performance": {
    "enable_tcp_nodelay": true,
    "enable_connection_reuse": true,
    "connection_warmup": true
  },
  "logging": {
    "level": "ERROR",
    "async_logging": true
  }
}
```

**优化要点**:
- 短超时时间（1 秒）
- 少重试次数（减少延迟）
- 最少连接策略（动态负载均衡）
- 预创建连接（连接预热）
- 启用 TCP_NODELAY
- 最小化日志

## 资源受限场景配置

### 资源受限服务端配置

**文件**: `server_resource_limited.json`

```json
{
  "server": {
    "listen_addr": "127.0.0.1",
    "listen_port": 8080,
    "max_connections": 500,
    "idle_timeout_seconds": 60,
    "worker_threads": 1
  },
  "connection_pool": {
    "min_connections": 1,
    "max_connections": 10,
    "idle_timeout_seconds": 30,
    "cleanup_interval_seconds": 15
  },
  "health_check": {
    "interval_seconds": 30,
    "timeout_seconds": 5,
    "failure_threshold": 5,
    "success_threshold": 3
  },
  "performance": {
    "buffer_pool_size": 100,
    "memory_pool_size": 500
  },
  "logging": {
    "level": "WARN",
    "file": "logs/server.log",
    "console": false,
    "max_file_size_mb": 10,
    "max_files": 3
  }
}
```

**优化要点**:
- 小连接数限制
- 单线程模式
- 小连接池
- 短空闲超时（快速释放资源）
- 宽松健康检测（减少开销）
- 小缓冲区池和内存池
- WARN 日志级别

## 完整配置示例

### 完整服务端配置

**文件**: `server_complete.json`

```json
{
  "server": {
    "listen_addr": "0.0.0.0",
    "listen_port": 8080,
    "max_connections": 10000,
    "idle_timeout_seconds": 300,
    "worker_threads": 4,
    "enable_tls": false,
    "tls_cert_file": "",
    "tls_key_file": ""
  },
  "connection_pool": {
    "min_connections": 5,
    "max_connections": 100,
    "idle_timeout_seconds": 240,
    "cleanup_interval_seconds": 30,
    "connection_timeout_seconds": 10,
    "enable_keepalive": true,
    "keepalive_interval_seconds": 60
  },
  "health_check": {
    "interval_seconds": 10,
    "timeout_seconds": 3,
    "failure_threshold": 3,
    "success_threshold": 2,
    "enable_active_check": true,
    "enable_passive_check": true
  },
  "service_registry": {
    "type": "local",
    "endpoints": [],
    "service_name": "my_service",
    "instance_weight": 100,
    "metadata": {
      "version": "1.0.0",
      "region": "us-west-1"
    }
  },
  "performance": {
    "buffer_pool_size": 1000,
    "memory_pool_size": 5000,
    "enable_zero_copy": false,
    "enable_tcp_nodelay": true,
    "enable_tcp_quickack": false,
    "send_buffer_size": 32768,
    "recv_buffer_size": 32768
  },
  "logging": {
    "level": "INFO",
    "file": "/var/log/frpc/server.log",
    "console": true,
    "max_file_size_mb": 100,
    "max_files": 10,
    "async_logging": false,
    "format": "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%n] %v"
  },
  "monitoring": {
    "enable_metrics": true,
    "metrics_port": 9090,
    "enable_tracing": false,
    "tracing_endpoint": ""
  }
}
```

### 完整客户端配置

**文件**: `client_complete.json`

```json
{
  "client": {
    "default_timeout_ms": 5000,
    "max_retries": 3,
    "retry_delay_ms": 100,
    "retry_backoff_multiplier": 2.0,
    "load_balance_strategy": "round_robin",
    "enable_circuit_breaker": false,
    "circuit_breaker_threshold": 10,
    "circuit_breaker_timeout_seconds": 60
  },
  "connection_pool": {
    "min_connections": 5,
    "max_connections": 100,
    "idle_timeout_seconds": 240,
    "cleanup_interval_seconds": 30,
    "connection_timeout_seconds": 10,
    "enable_keepalive": true,
    "keepalive_interval_seconds": 60
  },
  "service_discovery": {
    "type": "local",
    "endpoints": [],
    "refresh_interval_seconds": 30,
    "enable_cache": true,
    "cache_ttl_seconds": 300
  },
  "performance": {
    "enable_tcp_nodelay": true,
    "enable_connection_reuse": true,
    "connection_warmup": false,
    "max_concurrent_requests": 1000
  },
  "logging": {
    "level": "INFO",
    "file": "/var/log/frpc/client.log",
    "console": false,
    "max_file_size_mb": 100,
    "max_files": 10,
    "async_logging": false
  },
  "monitoring": {
    "enable_metrics": true,
    "enable_tracing": false
  }
}
```

## 配置文件加载

### C++ 代码示例

```cpp
#include <frpc/config.h>

// 加载服务端配置
auto server_config = ServerConfig::load_from_file("server_config.json");

// 加载客户端配置
auto client_config = ClientConfig::load_from_file("client_config.json");

// 使用配置
RpcServer server(server_config);
RpcClient client(client_config);
```

### 配置验证

```cpp
// 验证配置
if (!server_config.validate()) {
    std::cerr << "Invalid server configuration" << std::endl;
    return -1;
}

// 打印配置
server_config.print();
```

## 环境变量覆盖

支持通过环境变量覆盖配置文件中的参数：

```bash
# 覆盖监听端口
export FRPC_LISTEN_PORT=9090

# 覆盖最大连接数
export FRPC_MAX_CONNECTIONS=20000

# 覆盖日志级别
export FRPC_LOG_LEVEL=DEBUG

# 运行程序
./frpc_server --config server_config.json
```

## 配置最佳实践

### 1. 分环境配置

```
configs/
├── server_development.json
├── server_staging.json
├── server_production.json
├── client_development.json
├── client_staging.json
└── client_production.json
```

### 2. 使用配置模板

```bash
# 从模板生成配置
cp server_template.json server_config.json
# 编辑配置文件...
```

### 3. 配置版本控制

```bash
# 将配置文件加入版本控制
git add configs/*.json
git commit -m "Update production config"
```

### 4. 敏感信息处理

```json
{
  "database": {
    "password": "${DB_PASSWORD}"  // 从环境变量读取
  },
  "tls": {
    "cert_file": "${TLS_CERT_PATH}",
    "key_file": "${TLS_KEY_PATH}"
  }
}
```

## 参考资料

- [配置说明文档](../docs/configuration.md)
- [快速入门](../docs/quick_start.md)
- [最佳实践](../docs/best_practices.md)
