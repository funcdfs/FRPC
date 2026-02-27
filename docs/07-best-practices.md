# FRPC 框架最佳实践

本文档总结了使用 FRPC 框架的最佳实践和常见模式。

## 目录

1. [服务设计](#服务设计)
2. [错误处理](#错误处理)
3. [性能优化](#性能优化)
4. [资源管理](#资源管理)
5. [监控和运维](#监控和运维)
6. [安全性](#安全性)
7. [测试策略](#测试策略)

## 服务设计

### 1. 服务粒度

**推荐做法**:
```cpp
// ✓ 好：细粒度服务，职责单一
RpcTask<User> get_user(int user_id);
RpcTask<bool> update_user(int user_id, const User& user);
RpcTask<void> delete_user(int user_id);

// ✗ 差：粗粒度服务，职责混乱
RpcTask<Response> user_operation(const Request& req);
```

**原则**:
- 每个服务函数职责单一
- 避免过度拆分导致网络开销
- 遵循 RESTful 风格命名

### 2. 参数设计

**推荐做法**:
```cpp
// ✓ 好：使用结构体封装多个参数
struct CreateUserRequest {
    std::string name;
    int age;
    std::string email;
};

RpcTask<User> create_user(const CreateUserRequest& req);

// ✗ 差：参数过多
RpcTask<User> create_user(const std::string& name, int age, 
                          const std::string& email, const std::string& phone,
                          const std::string& address, ...);
```

**原则**:
- 参数超过 3 个时使用结构体
- 使用 const 引用传递大对象
- 提供默认值和可选参数

### 3. 返回值设计

**推荐做法**:
```cpp
// ✓ 好：使用 std::optional 表示可能失败
RpcTask<std::optional<User>> find_user(int user_id);

// ✓ 好：使用 Result 类型封装错误
template<typename T>
struct Result {
    std::optional<T> value;
    ErrorCode error_code;
    std::string error_message;
};

RpcTask<Result<User>> get_user(int user_id);

// ✗ 差：使用异常表示业务错误
RpcTask<User> get_user(int user_id) {
    if (!exists(user_id)) {
        throw std::runtime_error("User not found");  // 不推荐
    }
    co_return user;
}
```

**原则**:
- 使用 `std::optional` 表示可能不存在的值
- 异常用于系统错误，不用于业务错误
- 提供清晰的错误信息

### 4. 幂等性设计

**推荐做法**:
```cpp
// ✓ 好：幂等操作，可以安全重试
RpcTask<User> get_user(int user_id);  // 查询操作
RpcTask<void> set_user_status(int user_id, Status status);  // 设置操作

// ✗ 差：非幂等操作，重试可能导致问题
RpcTask<void> increment_counter(int user_id);  // 增量操作
RpcTask<void> add_points(int user_id, int points);  // 累加操作
```

**原则**:
- 查询操作天然幂等
- 更新操作使用绝对值而非增量
- 非幂等操作需要去重机制（如请求 ID）

## 错误处理

### 1. 异常处理

**推荐做法**:
```cpp
// ✓ 好：捕获特定异常，分别处理
RpcTask<void> call_service() {
    try {
        auto result = co_await client.call<int>("service", args);
        // 处理结果...
    } catch (const TimeoutException& e) {
        LOG_WARN("Request timeout, will retry");
        // 重试逻辑...
    } catch (const NetworkException& e) {
        LOG_ERROR("Network error: {}", e.what());
        // 故障转移...
    } catch (const ServiceException& e) {
        LOG_ERROR("Service error: {}", e.what());
        // 业务错误处理...
    }
}

// ✗ 差：捕获所有异常，无法区分错误类型
RpcTask<void> call_service() {
    try {
        auto result = co_await client.call<int>("service", args);
    } catch (const std::exception& e) {
        LOG_ERROR("Error: {}", e.what());
        // 无法针对性处理...
    }
}
```

### 2. 超时设置

**推荐做法**:
```cpp
// ✓ 好：根据服务特性设置不同超时
auto quick_result = co_await client.call<int>(
    "quick_service", args,
    std::chrono::milliseconds(1000)  // 快速服务 1 秒超时
);

auto slow_result = co_await client.call<Data>(
    "slow_service", args,
    std::chrono::milliseconds(30000)  // 慢速服务 30 秒超时
);

// ✗ 差：所有服务使用相同超时
ClientConfig config;
config.default_timeout = std::chrono::milliseconds(5000);
// 快速服务等待太久，慢速服务容易超时
```

### 3. 重试策略

**推荐做法**:
```cpp
// ✓ 好：指数退避重试
RpcTask<std::optional<int>> call_with_retry(int max_retries = 3) {
    int delay_ms = 100;
    
    for (int i = 0; i < max_retries; ++i) {
        try {
            auto result = co_await client.call<int>("service", args);
            co_return result;
        } catch (const NetworkException& e) {
            if (i == max_retries - 1) {
                LOG_ERROR("All retries failed");
                co_return std::nullopt;
            }
            
            LOG_WARN("Retry {} after {}ms", i + 1, delay_ms);
            co_await sleep(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;  // 指数退避
        }
    }
    
    co_return std::nullopt;
}

// ✗ 差：固定间隔重试，可能加剧服务压力
for (int i = 0; i < 3; ++i) {
    try {
        return co_await client.call<int>("service", args);
    } catch (...) {
        co_await sleep(std::chrono::milliseconds(100));  // 固定间隔
    }
}
```

## 性能优化

### 1. 连接复用

**推荐做法**:
```cpp
// ✓ 好：使用连接池自动复用
PoolConfig config;
config.min_connections = 10;  // 预创建连接
config.max_connections = 100;
ConnectionPool pool(config);

// 连接自动复用
auto conn = co_await pool.get_connection(instance);
// 使用连接...
pool.return_connection(std::move(conn));

// ✗ 差：每次创建新连接
for (int i = 0; i < 100; ++i) {
    auto conn = co_await create_connection(instance);  // 开销大
    // 使用连接...
    close_connection(conn);
}
```

### 2. 批量操作

**推荐做法**:
```cpp
// ✓ 好：批量发送请求
RpcTask<std::vector<User>> get_users_batch(const std::vector<int>& user_ids) {
    std::vector<RpcTask<User>> tasks;
    tasks.reserve(user_ids.size());
    
    for (int id : user_ids) {
        tasks.push_back(client.call<User>("get_user", id));
    }
    
    std::vector<User> results;
    for (auto& task : tasks) {
        results.push_back(co_await task);
    }
    
    co_return results;
}

// ✗ 差：逐个同步调用
RpcTask<std::vector<User>> get_users_sequential(const std::vector<int>& user_ids) {
    std::vector<User> results;
    for (int id : user_ids) {
        results.push_back(co_await client.call<User>("get_user", id));  // 串行
    }
    co_return results;
}
```

### 3. 缓存策略

**推荐做法**:
```cpp
// ✓ 好：使用缓存减少 RPC 调用
class UserService {
public:
    RpcTask<User> get_user(int user_id) {
        // 检查缓存
        if (auto cached = cache_.get(user_id)) {
            co_return *cached;
        }
        
        // 调用 RPC
        auto user = co_await client.call<User>("get_user", user_id);
        
        // 更新缓存
        if (user) {
            cache_.put(user_id, *user, std::chrono::minutes(5));
        }
        
        co_return user;
    }

private:
    LRUCache<int, User> cache_{1000};  // 缓存 1000 个用户
};
```

### 4. 预热连接

**推荐做法**:
```cpp
// ✓ 好：启动时预热连接池
RpcTask<void> warmup_connections() {
    auto instances = registry.get_instances("user_service");
    
    for (const auto& instance : instances) {
        // 预创建最小连接数
        for (int i = 0; i < pool_config.min_connections; ++i) {
            auto conn = co_await pool.get_connection(instance);
            pool.return_connection(std::move(conn));
        }
    }
    
    LOG_INFO("Connection pool warmed up");
}

int main() {
    // 启动时预热
    warmup_connections();
    
    // 开始处理请求...
}
```

## 资源管理

### 1. 连接管理

**推荐做法**:
```cpp
// ✓ 好：使用 RAII 管理连接
class ConnectionGuard {
public:
    ConnectionGuard(ConnectionPool& pool, const ServiceInstance& instance)
        : pool_(pool), conn_(co_await pool.get_connection(instance)) {}
    
    ~ConnectionGuard() {
        if (conn_) {
            pool_.return_connection(std::move(*conn_));
        }
    }
    
    Connection& get() { return *conn_; }

private:
    ConnectionPool& pool_;
    std::optional<Connection> conn_;
};

// 使用
{
    ConnectionGuard guard(pool, instance);
    co_await guard.get().send(data);
}  // 自动归还连接
```

### 2. 内存管理

**推荐做法**:
```cpp
// ✓ 好：使用智能指针管理内存
RpcTask<std::shared_ptr<Data>> load_large_data() {
    auto data = std::make_shared<Data>();
    // 加载数据...
    co_return data;  // 自动管理生命周期
}

// ✗ 差：手动管理内存
RpcTask<Data*> load_large_data() {
    auto data = new Data();  // 容易泄漏
    // 加载数据...
    co_return data;  // 谁负责释放？
}
```

### 3. 协程生命周期

**推荐做法**:
```cpp
// ✓ 好：确保协程完成或取消
class TaskManager {
public:
    void add_task(RpcTask<void> task) {
        tasks_.push_back(std::move(task));
    }
    
    RpcTask<void> wait_all() {
        for (auto& task : tasks_) {
            co_await task;
        }
        tasks_.clear();
    }
    
    ~TaskManager() {
        // 确保所有任务完成
        wait_all();
    }

private:
    std::vector<RpcTask<void>> tasks_;
};
```

## 监控和运维

### 1. 日志记录

**推荐做法**:
```cpp
// ✓ 好：记录关键信息和上下文
RpcTask<void> process_request(const Request& req) {
    LOG_INFO("Processing request: id={}, service={}", 
             req.request_id, req.service_name);
    
    auto start = std::chrono::steady_clock::now();
    
    try {
        auto result = co_await handle_request(req);
        
        auto duration = std::chrono::steady_clock::now() - start;
        LOG_INFO("Request completed: id={}, duration={}ms", 
                 req.request_id, 
                 std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    } catch (const std::exception& e) {
        LOG_ERROR("Request failed: id={}, error={}", 
                  req.request_id, e.what());
        throw;
    }
}

// ✗ 差：日志信息不足
RpcTask<void> process_request(const Request& req) {
    LOG_INFO("Processing request");  // 缺少上下文
    auto result = co_await handle_request(req);
    LOG_INFO("Done");  // 缺少详细信息
}
```

### 2. 指标收集

**推荐做法**:
```cpp
// ✓ 好：收集详细的性能指标
class MetricsCollector {
public:
    void record_request(const std::string& service, 
                       std::chrono::milliseconds latency,
                       bool success) {
        metrics_[service].total_requests++;
        if (success) {
            metrics_[service].successful_requests++;
        } else {
            metrics_[service].failed_requests++;
        }
        metrics_[service].total_latency += latency;
        
        // 更新 P99 延迟
        latency_histogram_[service].add(latency.count());
    }
    
    void report() {
        for (const auto& [service, metric] : metrics_) {
            LOG_INFO("Service: {}, QPS: {}, Success Rate: {:.2f}%, P99: {}ms",
                     service,
                     metric.total_requests / report_interval_seconds_,
                     metric.successful_requests * 100.0 / metric.total_requests,
                     latency_histogram_[service].percentile(99));
        }
    }
};
```

### 3. 健康检查

**推荐做法**:
```cpp
// ✓ 好：实现健康检查端点
RpcTask<HealthStatus> health_check() {
    HealthStatus status;
    
    // 检查数据库连接
    status.database_ok = co_await check_database();
    
    // 检查依赖服务
    status.dependencies_ok = co_await check_dependencies();
    
    // 检查资源使用
    status.memory_usage = get_memory_usage();
    status.cpu_usage = get_cpu_usage();
    
    status.healthy = status.database_ok && 
                     status.dependencies_ok &&
                     status.memory_usage < 0.9 &&
                     status.cpu_usage < 0.9;
    
    co_return status;
}
```

## 安全性

### 1. 输入验证

**推荐做法**:
```cpp
// ✓ 好：验证所有输入参数
RpcTask<User> create_user(const CreateUserRequest& req) {
    // 验证参数
    if (req.name.empty() || req.name.length() > 100) {
        throw std::invalid_argument("Invalid name");
    }
    
    if (req.age < 0 || req.age > 150) {
        throw std::invalid_argument("Invalid age");
    }
    
    if (!is_valid_email(req.email)) {
        throw std::invalid_argument("Invalid email");
    }
    
    // 处理请求...
}
```

### 2. 认证和授权

**推荐做法**:
```cpp
// ✓ 好：实现认证中间件
class AuthMiddleware {
public:
    RpcTask<bool> authenticate(const Request& req) {
        auto token = req.metadata["auth_token"];
        
        if (token.empty()) {
            LOG_WARN("Missing auth token");
            co_return false;
        }
        
        auto user = co_await verify_token(token);
        if (!user) {
            LOG_WARN("Invalid auth token");
            co_return false;
        }
        
        // 检查权限
        if (!user->has_permission(req.service_name)) {
            LOG_WARN("User {} lacks permission for {}", 
                     user->id, req.service_name);
            co_return false;
        }
        
        co_return true;
    }
};
```

### 3. 限流保护

**推荐做法**:
```cpp
// ✓ 好：实现令牌桶限流
class RateLimiter {
public:
    RateLimiter(int rate, int burst) 
        : rate_(rate), burst_(burst), tokens_(burst) {}
    
    bool try_acquire() {
        refill();
        
        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        
        return false;
    }

private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_refill_).count() / 1000.0;
        
        tokens_ = std::min(tokens_ + elapsed * rate_, static_cast<double>(burst_));
        last_refill_ = now;
    }
    
    int rate_;
    int burst_;
    double tokens_;
    std::chrono::steady_clock::time_point last_refill_;
};
```

## 测试策略

### 1. 单元测试

**推荐做法**:
```cpp
// ✓ 好：测试服务逻辑
TEST(UserServiceTest, CreateUser) {
    CreateUserRequest req{"Alice", 30, "alice@example.com"};
    
    auto result = create_user(req);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "Alice");
    EXPECT_EQ(result->age, 30);
}

TEST(UserServiceTest, InvalidInput) {
    CreateUserRequest req{"", -1, "invalid"};
    
    EXPECT_THROW(create_user(req), std::invalid_argument);
}
```

### 2. 集成测试

**推荐做法**:
```cpp
// ✓ 好：测试完整的 RPC 调用流程
TEST(IntegrationTest, EndToEndCall) {
    // 启动测试服务器
    TestServer server(8080);
    server.register_service("echo", echo_service);
    server.start();
    
    // 创建客户端
    ClientConfig config;
    RpcClient client(config);
    
    // 调用服务
    auto result = client.call<std::string>("echo", "Hello");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Hello");
    
    server.stop();
}
```

### 3. 性能测试

**推荐做法**:
```cpp
// ✓ 好：使用 Google Benchmark 测试性能
static void BM_RpcCall(benchmark::State& state) {
    RpcClient client(config);
    
    for (auto _ : state) {
        auto result = client.call<int>("add", 1, 2);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RpcCall);
```

## 总结

遵循这些最佳实践可以帮助您：
- 构建健壮可靠的 RPC 服务
- 提高系统性能和可扩展性
- 简化运维和故障排查
- 提升代码质量和可维护性

## 参考资料

- [快速入门](quick_start.md)
- [用户指南](user_guide.md)
- [配置说明](configuration.md)
- [故障排查](troubleshooting.md)
