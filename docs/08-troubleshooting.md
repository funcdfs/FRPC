# FRPC 框架故障排查指南

本文档提供常见问题的诊断和解决方法。

## 目录

1. [连接问题](#连接问题)
2. [超时问题](#超时问题)
3. [性能问题](#性能问题)
4. [内存问题](#内存问题)
5. [协程问题](#协程问题)
6. [配置问题](#配置问题)
7. [编译问题](#编译问题)
8. [调试技巧](#调试技巧)

## 连接问题

### 问题: 客户端无法连接到服务器

**症状**:
```
Error: Connection failed: Connection refused
```

**可能原因和解决方法**:

1. **服务器未启动**
   ```bash
   # 检查服务器进程
   ps aux | grep frpc_server
   
   # 检查端口监听
   netstat -tlnp | grep 8080
   # 或
   ss -tlnp | grep 8080
   ```

2. **端口被占用**
   ```bash
   # 查找占用端口的进程
   lsof -i :8080
   
   # 杀死占用进程或更换端口
   kill -9 <PID>
   ```

3. **防火墙阻止**
   ```bash
   # 检查防火墙规则
   sudo iptables -L -n
   
   # 允许端口
   sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
   
   # 或使用 firewalld
   sudo firewall-cmd --add-port=8080/tcp --permanent
   sudo firewall-cmd --reload
   ```

4. **IP 地址配置错误**
   ```cpp
   // 检查服务器配置
   ServerConfig config;
   config.listen_addr = "0.0.0.0";  // 监听所有网卡
   config.listen_port = 8080;
   
   // 检查客户端连接地址
   ServiceInstance instance{"127.0.0.1", 8080};  // 本地测试
   // 或
   ServiceInstance instance{"192.168.1.100", 8080};  // 远程服务器
   ```

### 问题: 连接频繁断开

**症状**:
```
Error: Connection closed unexpectedly
```

**可能原因和解决方法**:

1. **空闲超时设置过短**
   ```cpp
   // 增加空闲超时时间
   ServerConfig config;
   config.idle_timeout = std::chrono::seconds(300);  // 5 分钟
   
   PoolConfig pool_config;
   pool_config.idle_timeout = std::chrono::seconds(240);  // 4 分钟（小于服务端）
   ```

2. **网络不稳定**
   ```cpp
   // 启用心跳保活
   ClientConfig config;
   config.enable_keepalive = true;
   config.keepalive_interval = std::chrono::seconds(30);
   ```

3. **服务器资源耗尽**
   ```bash
   # 检查服务器资源
   top
   free -h
   df -h
   
   # 检查文件描述符限制
   ulimit -n
   
   # 增加限制
   ulimit -n 65535
   ```

## 超时问题

### 问题: 请求频繁超时

**症状**:
```
Error: Request timeout after 5000ms
```

**诊断步骤**:

1. **检查网络延迟**
   ```bash
   # 测试网络延迟
   ping <server_ip>
   
   # 测试 TCP 连接时间
   time telnet <server_ip> 8080
   ```

2. **检查服务端处理时间**
   ```cpp
   // 在服务端添加日志
   RpcTask<int> slow_service(int arg) {
       auto start = std::chrono::steady_clock::now();
       
       // 处理逻辑...
       
       auto duration = std::chrono::steady_clock::now() - start;
       LOG_INFO("Service processing time: {}ms", 
                std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
       
       co_return result;
   }
   ```

3. **调整超时设置**
   ```cpp
   // 根据实际情况调整超时
   ClientConfig config;
   config.default_timeout = std::chrono::milliseconds(10000);  // 10 秒
   
   // 或为特定服务设置更长超时
   auto result = co_await client.call<Data>(
       "slow_service", args,
       std::chrono::milliseconds(30000)  // 30 秒
   );
   ```

### 问题: 超时后服务端仍在处理

**症状**:
客户端超时返回，但服务端日志显示请求仍在处理。

**解决方法**:
```cpp
// 实现请求取消机制
class CancellableTask {
public:
    RpcTask<int> execute(std::chrono::milliseconds timeout) {
        std::atomic<bool> cancelled{false};
        
        // 启动超时定时器
        auto timer = start_timer(timeout, [&cancelled]() {
            cancelled = true;
        });
        
        // 执行任务，定期检查取消标志
        while (!cancelled) {
            // 处理逻辑...
            if (should_check_cancellation()) {
                if (cancelled) {
                    LOG_INFO("Task cancelled due to timeout");
                    co_return -1;
                }
            }
        }
        
        co_return result;
    }
};
```

## 性能问题

### 问题: QPS 低于预期

**诊断步骤**:

1. **检查 CPU 使用率**
   ```bash
   # 查看 CPU 使用情况
   top -H -p <pid>
   
   # 使用 perf 分析热点
   perf record -g -p <pid>
   perf report
   ```

2. **检查连接池配置**
   ```cpp
   // 增加连接池大小
   PoolConfig config;
   config.min_connections = 20;
   config.max_connections = 200;
   
   // 检查连接复用率
   auto stats = pool.get_stats();
   LOG_INFO("Connection reuse rate: {:.2f}%", 
            stats.connection_reuse_rate * 100);
   ```

3. **检查序列化性能**
   ```cpp
   // 使用性能分析工具
   auto start = std::chrono::steady_clock::now();
   auto data = serializer.serialize(request);
   auto duration = std::chrono::steady_clock::now() - start;
   
   LOG_INFO("Serialization time: {}μs", 
            std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
   ```

4. **检查网络带宽**
   ```bash
   # 使用 iftop 监控网络流量
   sudo iftop -i eth0
   
   # 使用 iperf 测试带宽
   # 服务端
   iperf -s
   # 客户端
   iperf -c <server_ip>
   ```

### 问题: 延迟过高

**诊断步骤**:

1. **分析延迟来源**
   ```cpp
   // 添加详细的时间戳日志
   RpcTask<void> process_request(const Request& req) {
       auto t1 = std::chrono::steady_clock::now();
       
       // 序列化
       auto data = serialize(req);
       auto t2 = std::chrono::steady_clock::now();
       
       // 网络发送
       co_await send(data);
       auto t3 = std::chrono::steady_clock::now();
       
       // 等待响应
       auto response = co_await recv();
       auto t4 = std::chrono::steady_clock::now();
       
       // 反序列化
       auto result = deserialize(response);
       auto t5 = std::chrono::steady_clock::now();
       
       LOG_INFO("Latency breakdown: serialize={}μs, send={}μs, wait={}μs, deserialize={}μs",
                duration_us(t1, t2), duration_us(t2, t3), 
                duration_us(t3, t4), duration_us(t4, t5));
   }
   ```

2. **优化热点代码**
   ```cpp
   // 使用缓存减少重复计算
   // 使用对象池减少内存分配
   // 使用批量操作减少网络往返
   ```

## 内存问题

### 问题: 内存泄漏

**诊断步骤**:

1. **使用 AddressSanitizer**
   ```bash
   # 编译时启用 ASan
   cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
   make
   
   # 运行程序
   ./frpc_server
   
   # 查看泄漏报告
   ```

2. **使用 Valgrind**
   ```bash
   # 检测内存泄漏
   valgrind --leak-check=full --show-leak-kinds=all ./frpc_server
   
   # 生成详细报告
   valgrind --leak-check=full --log-file=valgrind.log ./frpc_server
   ```

3. **检查协程生命周期**
   ```cpp
   // 确保协程正确销毁
   class CoroutineManager {
   public:
       ~CoroutineManager() {
           // 等待所有协程完成
           for (auto& task : tasks_) {
               task.wait();
           }
       }
   };
   ```

### 问题: 内存使用过高

**诊断步骤**:

1. **检查内存池配置**
   ```cpp
   // 查看内存池统计
   auto stats = memory_pool.get_stats();
   LOG_INFO("Memory pool: total={}, free={}, usage={:.2f}%",
            stats.total_blocks, stats.free_blocks,
            (stats.total_blocks - stats.free_blocks) * 100.0 / stats.total_blocks);
   ```

2. **检查缓冲区池**
   ```cpp
   // 减少缓冲区大小或数量
   BufferPool pool(4096, 50);  // 4KB, 50 个缓冲区
   ```

3. **使用内存分析工具**
   ```bash
   # 使用 heaptrack 分析内存分配
   heaptrack ./frpc_server
   heaptrack_gui heaptrack.frpc_server.<pid>.gz
   ```

## 协程问题

### 问题: 协程死锁

**症状**:
程序挂起，没有响应。

**诊断步骤**:

1. **检查协程状态**
   ```cpp
   // 添加协程状态监控
   auto state = scheduler.get_state(handle);
   LOG_INFO("Coroutine state: {}", state_to_string(state));
   ```

2. **检查循环依赖**
   ```cpp
   // ✗ 错误：协程 A 等待协程 B，协程 B 等待协程 A
   RpcTask<int> coroutine_a() {
       auto result = co_await coroutine_b();  // 等待 B
       co_return result;
   }
   
   RpcTask<int> coroutine_b() {
       auto result = co_await coroutine_a();  // 等待 A（死锁！）
       co_return result;
   }
   ```

3. **使用超时保护**
   ```cpp
   // ✓ 正确：添加超时保护
   RpcTask<std::optional<int>> safe_call() {
       try {
           auto result = co_await call_with_timeout(
               coroutine_a(),
               std::chrono::seconds(10)
           );
           co_return result;
       } catch (const TimeoutException&) {
           LOG_ERROR("Coroutine timeout, possible deadlock");
           co_return std::nullopt;
       }
   }
   ```

### 问题: 协程崩溃

**症状**:
```
Segmentation fault (core dumped)
```

**诊断步骤**:

1. **启用核心转储**
   ```bash
   # 启用核心转储
   ulimit -c unlimited
   
   # 运行程序
   ./frpc_server
   
   # 分析核心转储
   gdb ./frpc_server core
   (gdb) bt
   (gdb) info locals
   ```

2. **检查悬空引用**
   ```cpp
   // ✗ 错误：引用局部变量
   RpcTask<int> dangerous() {
       int local_var = 42;
       co_await async_operation();
       return local_var;  // local_var 可能已失效
   }
   
   // ✓ 正确：使用值传递或智能指针
   RpcTask<int> safe() {
       auto shared_var = std::make_shared<int>(42);
       co_await async_operation();
       return *shared_var;
   }
   ```

## 配置问题

### 问题: 配置文件加载失败

**症状**:
```
Error: Failed to load config file: config.json
```

**解决方法**:

1. **检查文件路径**
   ```bash
   # 检查文件是否存在
   ls -l config.json
   
   # 检查文件权限
   chmod 644 config.json
   ```

2. **验证 JSON 格式**
   ```bash
   # 使用 jq 验证 JSON
   jq . config.json
   
   # 或使用 Python
   python3 -m json.tool config.json
   ```

3. **检查配置值**
   ```cpp
   // 添加配置验证
   ServerConfig config = ServerConfig::load_from_file("config.json");
   
   if (config.listen_port == 0) {
       LOG_ERROR("Invalid port configuration");
       return -1;
   }
   
   if (config.max_connections < 1) {
       LOG_ERROR("Invalid max_connections configuration");
       return -1;
   }
   ```

## 编译问题

### 问题: C++20 协程编译失败

**症状**:
```
error: 'coroutine' is not a member of 'std'
```

**解决方法**:

1. **检查编译器版本**
   ```bash
   # GCC
   g++ --version  # 需要 GCC 10+
   
   # Clang
   clang++ --version  # 需要 Clang 11+
   ```

2. **启用 C++20 标准**
   ```cmake
   # CMakeLists.txt
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   ```

3. **添加编译选项**
   ```cmake
   # GCC
   add_compile_options(-fcoroutines)
   
   # Clang
   add_compile_options(-fcoroutines-ts)
   ```

### 问题: 链接错误

**症状**:
```
undefined reference to `frpc::NetworkEngine::run()'
```

**解决方法**:

1. **检查库链接**
   ```cmake
   target_link_libraries(my_app
       frpc
       pthread
   )
   ```

2. **检查库路径**
   ```bash
   # 检查库文件
   ls -l build/libfrpc.a
   
   # 设置库路径
   export LD_LIBRARY_PATH=/path/to/frpc/build:$LD_LIBRARY_PATH
   ```

## 调试技巧

### 1. 启用详细日志

```cpp
// 设置日志级别为 DEBUG
Logger::set_level(LogLevel::DEBUG);

// 记录详细的调试信息
LOG_DEBUG("Connection state: fd={}, state={}", fd, state);
LOG_DEBUG("Request: id={}, service={}, payload_size={}", 
          req.request_id, req.service_name, req.payload.size());
```

### 2. 使用 GDB 调试

```bash
# 编译时启用调试符号
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# 使用 GDB
gdb ./frpc_server

# 常用命令
(gdb) break main
(gdb) run
(gdb) next
(gdb) step
(gdb) print variable
(gdb) backtrace
```

### 3. 使用 strace 跟踪系统调用

```bash
# 跟踪系统调用
strace -f -e trace=network ./frpc_server

# 跟踪特定进程
strace -p <pid>

# 保存到文件
strace -o trace.log ./frpc_server
```

### 4. 使用 tcpdump 抓包

```bash
# 抓取指定端口的包
sudo tcpdump -i any port 8080 -w capture.pcap

# 使用 Wireshark 分析
wireshark capture.pcap
```

### 5. 性能分析

```bash
# 使用 perf 分析 CPU
perf record -g ./frpc_server
perf report

# 使用火焰图
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

## 常见错误代码

| 错误码 | 说明 | 解决方法 |
|--------|------|----------|
| 1001 | 连接失败 | 检查服务器地址和端口 |
| 1005 | 请求超时 | 增加超时时间或优化服务性能 |
| 2001 | 反序列化失败 | 检查数据格式和协议版本 |
| 3000 | 服务未找到 | 检查服务名称是否正确注册 |
| 3002 | 无可用实例 | 检查服务注册和健康检测 |
| 5001 | 内存不足 | 增加系统内存或优化内存使用 |

## 获取帮助

如果以上方法无法解决问题：

1. 查看[用户指南](user_guide.md)和[最佳实践](best_practices.md)
2. 搜索 GitHub Issues: https://github.com/your-org/frpc-framework/issues
3. 提交新 Issue 并提供：
   - 详细的错误信息和日志
   - 复现步骤
   - 环境信息（操作系统、编译器版本等）
   - 配置文件
4. 加入社区讨论组

## 参考资料

- [快速入门](02-quick-start.md)
- [用户指南](04-user-guide.md)
- [配置说明](05-configuration.md)
- [最佳实践](07-best-practices.md)
