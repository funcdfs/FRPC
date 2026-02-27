# 健康检测示例说明

本示例演示 FRPC 框架的健康检测和故障转移功能。

## 功能特性

1. **定期健康检测**: 自动检测服务实例健康状态
2. **故障检测**: 识别不健康的服务实例
3. **自动移除**: 将不健康实例从服务列表中移除
4. **自动恢复**: 实例恢复后自动重新加入
5. **故障转移**: 客户端自动切换到健康实例

## 架构图

```
┌──────────────────┐
│ Health Checker   │
│                  │
│ ┌──────────────┐ │
│ │Check Loop    │ │
│ │Every 10s     │ │
│ └──────────────┘ │
└────────┬─────────┘
         │
         ├─────────────┬─────────────┐
         │             │             │
         ▼             ▼             ▼
    ┌────────┐    ┌────────┐    ┌────────┐
    │Server 1│    │Server 2│    │Server 3│
    │Healthy │    │Healthy │    │ Down   │
    │  ✓     │    │  ✓     │    │  ✗     │
    └────────┘    └────────┘    └────────┘
         │             │
         └──────┬──────┘
                │
         ┌──────▼──────┐
         │   Client    │
         │(Only uses   │
         │ healthy     │
         │ instances)  │
         └─────────────┘
```

## 健康检测流程

### 1. 检测循环

```
┌─────────────────────────────────────┐
│ Health Check Loop (Every 10s)       │
└─────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────┐
│ For each registered instance:       │
│   1. Send health check request      │
│   2. Wait for response (timeout 3s) │
│   3. Update health status           │
└─────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────┐
│ Update Service Registry             │
│   - Mark healthy instances          │
│   - Mark unhealthy instances        │
│   - Notify subscribers              │
└─────────────────────────────────────┘
```

### 2. 状态转换

```
         ┌─────────┐
         │ Healthy │
         └────┬────┘
              │
    Fail 1x   │   Success
    ──────────┼──────────
              │
         ┌────▼────┐
         │ Healthy │
         │ (1 fail)│
         └────┬────┘
              │
    Fail 2x   │   Success
    ──────────┼──────────
              │
         ┌────▼────┐
         │ Healthy │
         │ (2 fail)│
         └────┬────┘
              │
    Fail 3x   │   Success
    ──────────┼──────────
              │
         ┌────▼────────┐
         │  Unhealthy  │◄─── Removed from service list
         └────┬────────┘
              │
    Success 1x│
    ──────────┼
              │
         ┌────▼────────┐
         │  Unhealthy  │
         │ (1 success) │
         └────┬────────┘
              │
    Success 2x│
    ──────────┼
              │
         ┌────▼────┐
         │ Healthy │◄─── Added back to service list
         └─────────┘
```

## 配置参数

### 健康检测配置

```cpp
HealthCheckConfig config;

// 检测间隔：每 10 秒检测一次
config.interval = std::chrono::seconds(10);

// 检测超时：3 秒内未响应视为失败
config.timeout = std::chrono::seconds(3);

// 失败阈值：连续失败 3 次标记为不健康
config.failure_threshold = 3;

// 成功阈值：连续成功 2 次标记为健康
config.success_threshold = 2;
```

### 参数说明

| 参数 | 默认值 | 说明 | 推荐值 |
|------|--------|------|--------|
| interval | 10s | 检测间隔 | 5-30s |
| timeout | 3s | 检测超时 | 2-5s |
| failure_threshold | 3 | 失败阈值 | 2-5 |
| success_threshold | 2 | 成功阈值 | 1-3 |

## 运行示例

### 步骤 1: 启动服务实例

```bash
# 终端 1: 启动实例 1
./health_check_server --port 8080

# 终端 2: 启动实例 2
./health_check_server --port 8081

# 终端 3: 启动实例 3
./health_check_server --port 8082
```

### 步骤 2: 启动客户端（包含健康检测）

```bash
# 终端 4: 启动客户端
./health_check_client
```

### 步骤 3: 模拟故障

```bash
# 杀死实例 1
kill <pid_of_8080>

# 观察客户端日志
# 预期：
# - 健康检测器检测到实例 1 失败
# - 连续失败 3 次后标记为不健康
# - 客户端自动切换到实例 2 和 3
```

### 步骤 4: 模拟恢复

```bash
# 重新启动实例 1
./health_check_server --port 8080

# 观察客户端日志
# 预期：
# - 健康检测器检测到实例 1 恢复
# - 连续成功 2 次后标记为健康
# - 实例 1 重新加入服务列表
# - 客户端开始向实例 1 发送请求
```

## 预期输出

### 健康检测器输出

```
=== Health Checker Started ===
Interval: 10s
Timeout: 3s
Failure threshold: 3
Success threshold: 2

Monitoring 3 instances:
  - 127.0.0.1:8080
  - 127.0.0.1:8081
  - 127.0.0.1:8082

[10:00:00] Checking instance 127.0.0.1:8080... ✓ Healthy
[10:00:00] Checking instance 127.0.0.1:8081... ✓ Healthy
[10:00:00] Checking instance 127.0.0.1:8082... ✓ Healthy

[10:00:10] Checking instance 127.0.0.1:8080... ✗ Failed (1/3)
[10:00:10] Checking instance 127.0.0.1:8081... ✓ Healthy
[10:00:10] Checking instance 127.0.0.1:8082... ✓ Healthy

[10:00:20] Checking instance 127.0.0.1:8080... ✗ Failed (2/3)
[10:00:20] Checking instance 127.0.0.1:8081... ✓ Healthy
[10:00:20] Checking instance 127.0.0.1:8082... ✓ Healthy

[10:00:30] Checking instance 127.0.0.1:8080... ✗ Failed (3/3)
[10:00:30] Instance 127.0.0.1:8080 marked as UNHEALTHY
[10:00:30] Notifying service registry...
[10:00:30] Checking instance 127.0.0.1:8081... ✓ Healthy
[10:00:30] Checking instance 127.0.0.1:8082... ✓ Healthy

Available instances: 2 (8081, 8082)
```

### 客户端输出

```
=== Health Check Client Started ===

Discovered 3 service instances:
  - 127.0.0.1:8080 (Healthy)
  - 127.0.0.1:8081 (Healthy)
  - 127.0.0.1:8082 (Healthy)

Sending requests...

[10:00:05] Request 1 → 127.0.0.1:8080 ✓
[10:00:06] Request 2 → 127.0.0.1:8081 ✓
[10:00:07] Request 3 → 127.0.0.1:8082 ✓

[10:00:35] Service instances updated!
Available instances: 2
  - 127.0.0.1:8081 (Healthy)
  - 127.0.0.1:8082 (Healthy)

[10:00:36] Request 4 → 127.0.0.1:8081 ✓
[10:00:37] Request 5 → 127.0.0.1:8082 ✓
[10:00:38] Request 6 → 127.0.0.1:8081 ✓

[10:01:05] Service instances updated!
Available instances: 3
  - 127.0.0.1:8080 (Healthy) ← Recovered!
  - 127.0.0.1:8081 (Healthy)
  - 127.0.0.1:8082 (Healthy)

[10:01:06] Request 7 → 127.0.0.1:8080 ✓
```

## 实现细节

### 健康检测器实现

```cpp
class HealthChecker {
public:
    HealthChecker(const HealthCheckConfig& config, 
                  ServiceRegistry* registry)
        : config_(config), registry_(registry) {}
    
    void start() {
        running_ = true;
        check_thread_ = std::thread([this]() {
            check_loop();
        });
    }
    
    void stop() {
        running_ = false;
        if (check_thread_.joinable()) {
            check_thread_.join();
        }
    }
    
private:
    RpcTask<void> check_loop() {
        while (running_) {
            for (auto& target : targets_) {
                bool healthy = co_await check_instance(target.instance);
                
                if (healthy) {
                    target.consecutive_failures = 0;
                    target.consecutive_successes++;
                    
                    if (target.consecutive_successes >= config_.success_threshold) {
                        registry_->update_health_status(
                            target.service_name, 
                            target.instance, 
                            true
                        );
                    }
                } else {
                    target.consecutive_successes = 0;
                    target.consecutive_failures++;
                    
                    if (target.consecutive_failures >= config_.failure_threshold) {
                        registry_->update_health_status(
                            target.service_name, 
                            target.instance, 
                            false
                        );
                    }
                }
            }
            
            co_await sleep(config_.interval);
        }
    }
    
    RpcTask<bool> check_instance(const ServiceInstance& instance) {
        try {
            auto conn = co_await connect_with_timeout(
                instance, 
                config_.timeout
            );
            
            // 发送健康检测请求
            co_await conn.send(health_check_request());
            auto response = co_await conn.recv();
            
            co_return response.is_ok();
        } catch (const std::exception& e) {
            LOG_WARN("Health check failed for {}:{}: {}", 
                     instance.host, instance.port, e.what());
            co_return false;
        }
    }
};
```

## 故障场景测试

### 场景 1: 单实例故障

```bash
# 3 个实例运行中
# 杀死 1 个实例
kill <pid>

# 预期结果：
# - 30 秒后（3 次检测）实例被标记为不健康
# - 客户端请求分配到剩余 2 个实例
# - 无请求失败
```

### 场景 2: 多实例故障

```bash
# 3 个实例运行中
# 杀死 2 个实例
kill <pid1> <pid2>

# 预期结果：
# - 30 秒后 2 个实例被标记为不健康
# - 所有请求分配到剩余 1 个实例
# - 无请求失败
```

### 场景 3: 全部实例故障

```bash
# 3 个实例运行中
# 杀死所有实例
killall health_check_server

# 预期结果：
# - 30 秒后所有实例被标记为不健康
# - 客户端请求失败，返回 NoInstanceAvailable 错误
```

### 场景 4: 网络抖动

```bash
# 使用 tc 模拟网络延迟
sudo tc qdisc add dev lo root netem delay 5000ms

# 预期结果：
# - 健康检测超时（3 秒）
# - 但不会立即标记为不健康（需要连续 3 次失败）
# - 恢复网络后实例保持健康状态
```

## 性能影响

### 健康检测开销

| 实例数 | 检测间隔 | CPU 使用 | 网络带宽 | 内存使用 |
|--------|----------|----------|----------|----------|
| 10 | 10s | <1% | <1KB/s | <1MB |
| 100 | 10s | <2% | <10KB/s | <10MB |
| 1000 | 10s | <5% | <100KB/s | <100MB |

### 优化建议

1. **调整检测间隔**: 根据服务稳定性调整
   - 稳定服务: 30-60 秒
   - 不稳定服务: 5-10 秒

2. **批量检测**: 并发检测多个实例
   ```cpp
   std::vector<RpcTask<bool>> tasks;
   for (const auto& target : targets_) {
       tasks.push_back(check_instance(target.instance));
   }
   // 并发等待所有检测完成
   ```

3. **智能检测**: 根据历史健康状态调整检测频率
   ```cpp
   if (instance.always_healthy) {
       interval = 60s;  // 降低检测频率
   } else {
       interval = 10s;  // 保持高频检测
   }
   ```

## 最佳实践

### 1. 合理设置阈值

```cpp
// 快速故障检测（适合关键服务）
config.interval = std::chrono::seconds(5);
config.failure_threshold = 2;

// 宽松故障检测（适合网络不稳定环境）
config.interval = std::chrono::seconds(30);
config.failure_threshold = 5;
```

### 2. 实现健康检测端点

```cpp
// 服务端实现健康检测接口
RpcTask<HealthStatus> health_check() {
    HealthStatus status;
    
    // 检查数据库连接
    status.database_ok = check_database();
    
    // 检查依赖服务
    status.dependencies_ok = check_dependencies();
    
    // 检查资源使用
    status.cpu_usage = get_cpu_usage();
    status.memory_usage = get_memory_usage();
    
    status.healthy = status.database_ok && 
                     status.dependencies_ok &&
                     status.cpu_usage < 0.9 &&
                     status.memory_usage < 0.9;
    
    co_return status;
}
```

### 3. 监控健康检测

```cpp
// 记录健康检测统计
struct HealthCheckStats {
    uint64_t total_checks = 0;
    uint64_t successful_checks = 0;
    uint64_t failed_checks = 0;
    uint64_t instances_marked_unhealthy = 0;
    uint64_t instances_recovered = 0;
};

// 定期输出统计信息
void report_stats() {
    LOG_INFO("Health Check Stats:");
    LOG_INFO("  Total checks: {}", stats.total_checks);
    LOG_INFO("  Success rate: {:.2f}%", 
             stats.successful_checks * 100.0 / stats.total_checks);
    LOG_INFO("  Instances marked unhealthy: {}", 
             stats.instances_marked_unhealthy);
    LOG_INFO("  Instances recovered: {}", 
             stats.instances_recovered);
}
```

## 常见问题

### Q: 健康检测失败但实例实际正常？

A: 可能原因：
1. 超时设置过短
2. 网络抖动
3. 实例负载过高

解决方法：
- 增加超时时间
- 增加失败阈值
- 优化健康检测端点性能

### Q: 实例恢复后多久重新加入？

A: 取决于配置：
```
恢复时间 = interval × success_threshold
默认: 10s × 2 = 20s
```

### Q: 如何实现主动健康检测？

A: 除了定期检测，还可以在请求失败时立即检测：

```cpp
try {
    auto result = co_await client.call<int>("service", args);
} catch (const NetworkException& e) {
    // 立即触发健康检测
    health_checker.check_now(instance);
}
```

## 参考资料

- [用户指南 - 健康检测](../docs/user_guide.md#健康检测)
- [配置说明 - 健康检测配置](../docs/configuration.md#健康检测配置)
- [最佳实践](../docs/best_practices.md)
