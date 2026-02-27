/**
 * @file integration_example.cpp
 * @brief 完整的 RPC 调用示例 - 演示服务注册、发现、负载均衡、健康检测
 * 
 * 本示例展示 FRPC 框架的完整功能：
 * 1. 服务注册：多个服务实例注册到服务注册中心
 * 2. 服务发现：客户端从注册中心查询可用实例
 * 3. 负载均衡：使用不同策略（轮询、随机、加权轮询）分发请求
 * 4. 健康检测：自动检测实例健康状态，移除不健康实例
 * 5. 故障转移：当实例失败时自动切换到其他实例
 * 
 * 需求覆盖：
 * - 需求 15.3: 演示完整的 RPC 调用流程
 * - 需求 15.8: 提供详细的代码注释和使用说明
 * - 需求 4.7: 服务端并发处理多个请求
 * - 需求 8.4: 服务发现机制
 * - 需求 9.4: 健康检测和故障转移
 * - 需求 10.1: 负载均衡
 */

#include "frpc/rpc_server.h"
#include "frpc/rpc_client.h"
#include "frpc/service_registry.h"
#include "frpc/load_balancer.h"
#include "frpc/health_checker.h"
#include "frpc/serializer.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include <cstring>

using namespace frpc;

// ============================================================================
// 辅助函数：序列化和反序列化
// ============================================================================

/**
 * @brief 序列化整数（大端序）
 */
ByteBuffer serialize_int(int32_t value) {
    ByteBuffer buffer(4);
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
    return buffer;
}

/**
 * @brief 反序列化整数（大端序）
 */
int32_t deserialize_int(const ByteBuffer& buffer) {
    if (buffer.size() < 4) return 0;
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/**
 * @brief 序列化字符串（长度前缀）
 */
ByteBuffer serialize_string(const std::string& str) {
    ByteBuffer buffer;
    auto len = serialize_int(static_cast<int32_t>(str.size()));
    buffer.insert(buffer.end(), len.begin(), len.end());
    buffer.insert(buffer.end(), str.begin(), str.end());
    return buffer;
}

/**
 * @brief 反序列化字符串（长度前缀）
 */
std::string deserialize_string(const ByteBuffer& buffer, size_t& offset) {
    if (offset + 4 > buffer.size()) return "";
    
    ByteBuffer len_bytes(buffer.begin() + offset, buffer.begin() + offset + 4);
    int32_t len = deserialize_int(len_bytes);
    offset += 4;
    
    if (offset + len > buffer.size()) return "";
    
    std::string result(buffer.begin() + offset, buffer.begin() + offset + len);
    offset += len;
    return result;
}

// ============================================================================
// 示例服务：计算服务
// ============================================================================

/**
 * @brief 加法服务处理器
 * 
 * 接收两个整数，返回它们的和。
 * Payload 格式：[a(4 bytes)][b(4 bytes)]
 */
Response handle_add(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    if (req.payload.size() < 8) {
        resp.error_code = ErrorCode::InvalidArgument;
        resp.error_message = "Invalid payload: expected 8 bytes";
        return resp;
    }
    
    ByteBuffer a_bytes(req.payload.begin(), req.payload.begin() + 4);
    ByteBuffer b_bytes(req.payload.begin() + 4, req.payload.begin() + 8);
    
    int32_t a = deserialize_int(a_bytes);
    int32_t b = deserialize_int(b_bytes);
    int32_t result = a + b;
    
    std::cout << "[Service] add(" << a << ", " << b << ") = " << result << std::endl;
    
    resp.error_code = ErrorCode::Success;
    resp.payload = serialize_int(result);
    
    return resp;
}

/**
 * @brief 乘法服务处理器
 * 
 * 接收两个整数，返回它们的积。
 * Payload 格式：[a(4 bytes)][b(4 bytes)]
 */
Response handle_multiply(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    if (req.payload.size() < 8) {
        resp.error_code = ErrorCode::InvalidArgument;
        resp.error_message = "Invalid payload: expected 8 bytes";
        return resp;
    }
    
    ByteBuffer a_bytes(req.payload.begin(), req.payload.begin() + 4);
    ByteBuffer b_bytes(req.payload.begin() + 4, req.payload.begin() + 8);
    
    int32_t a = deserialize_int(a_bytes);
    int32_t b = deserialize_int(b_bytes);
    int32_t result = a * b;
    
    std::cout << "[Service] multiply(" << a << ", " << b << ") = " << result << std::endl;
    
    resp.error_code = ErrorCode::Success;
    resp.payload = serialize_int(result);
    
    return resp;
}

/**
 * @brief Echo 服务处理器
 * 
 * 返回接收到的相同数据。
 */
Response handle_echo(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    resp.error_code = ErrorCode::Success;
    resp.payload = req.payload;
    
    std::cout << "[Service] echo(" << req.payload.size() << " bytes)" << std::endl;
    
    return resp;
}

// ============================================================================
// 服务实例模拟器
// ============================================================================

/**
 * @brief 服务实例模拟器
 * 
 * 模拟一个独立的服务实例，包含服务端和实例信息。
 * 在实际应用中，每个实例会运行在不同的进程或机器上。
 */
class ServiceInstanceSimulator {
public:
    ServiceInstanceSimulator(const std::string& name, 
                            const std::string& host, 
                            uint16_t port,
                            int weight = 100)
        : name_(name), instance_{host, port, weight}, server_(std::make_unique<RpcServer>()) {
        
        // 注册服务
        server_->register_service("calc.add", handle_add);
        server_->register_service("calc.multiply", handle_multiply);
        server_->register_service("calc.echo", handle_echo);
        
        std::cout << "[Instance] Created " << name_ << " at " 
                  << host << ":" << port << " (weight=" << weight << ")" << std::endl;
    }
    
    const std::string& name() const { return name_; }
    const ServiceInstance& instance() const { return instance_; }
    RpcServer* server() { return server_.get(); }
    
    /**
     * @brief 处理原始请求
     */
    std::optional<ByteBuffer> handle_request(const ByteBuffer& request) {
        return server_->handle_raw_request(request);
    }

private:
    std::string name_;
    ServiceInstance instance_;
    std::unique_ptr<RpcServer> server_;
};

// ============================================================================
// 示例 1: 服务注册和发现
// ============================================================================

void example_service_registration_and_discovery() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  示例 1: 服务注册和发现                                   ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // 创建服务注册中心
    ServiceRegistry registry;
    
    // 创建多个服务实例
    ServiceInstance instance1{"192.168.1.101", 8080, 100};
    ServiceInstance instance2{"192.168.1.102", 8080, 100};
    ServiceInstance instance3{"192.168.1.103", 8080, 100};
    
    std::cout << "步骤 1: 注册服务实例" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 注册服务实例
    registry.register_service("calc_service", instance1);
    std::cout << "✓ 注册实例 1: " << instance1.host << ":" << instance1.port << std::endl;
    
    registry.register_service("calc_service", instance2);
    std::cout << "✓ 注册实例 2: " << instance2.host << ":" << instance2.port << std::endl;
    
    registry.register_service("calc_service", instance3);
    std::cout << "✓ 注册实例 3: " << instance3.host << ":" << instance3.port << std::endl;
    
    std::cout << "\n步骤 2: 查询服务实例" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 查询服务实例
    auto instances = registry.get_instances("calc_service");
    std::cout << "✓ 查询到 " << instances.size() << " 个实例：" << std::endl;
    for (size_t i = 0; i < instances.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << instances[i].host << ":" 
                  << instances[i].port << " (权重: " << instances[i].weight << ")" << std::endl;
    }
    
    std::cout << "\n步骤 3: 订阅服务变更通知" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 订阅服务变更
    registry.subscribe("calc_service", [](const std::vector<ServiceInstance>& instances) {
        std::cout << "✓ [通知] 服务实例列表已更新，当前实例数: " << instances.size() << std::endl;
    });
    
    std::cout << "✓ 已订阅 calc_service 的变更通知" << std::endl;
    
    std::cout << "\n步骤 4: 注销一个实例" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 注销实例
    registry.unregister_service("calc_service", instance2);
    std::cout << "✓ 注销实例 2: " << instance2.host << ":" << instance2.port << std::endl;
    
    // 再次查询
    instances = registry.get_instances("calc_service");
    std::cout << "✓ 当前实例数: " << instances.size() << std::endl;
}

// ============================================================================
// 示例 2: 负载均衡策略
// ============================================================================

void example_load_balancing() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  示例 2: 负载均衡策略                                     ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // 准备服务实例列表
    std::vector<ServiceInstance> instances = {
        {"192.168.1.101", 8080, 100},
        {"192.168.1.102", 8080, 100},
        {"192.168.1.103", 8080, 100}
    };
    
    // 测试轮询负载均衡
    std::cout << "策略 1: 轮询负载均衡 (Round-Robin)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    {
        RoundRobinLoadBalancer lb;
        std::cout << "连续 5 次选择：" << std::endl;
        for (int i = 0; i < 5; ++i) {
            auto selected = lb.select(instances);
            std::cout << "  " << (i + 1) << ". " << selected.host << ":" << selected.port << std::endl;
        }
    }
    
    // 测试随机负载均衡
    std::cout << "\n策略 2: 随机负载均衡 (Random)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    {
        RandomLoadBalancer lb;
        std::cout << "连续 5 次选择：" << std::endl;
        for (int i = 0; i < 5; ++i) {
            auto selected = lb.select(instances);
            std::cout << "  " << (i + 1) << ". " << selected.host << ":" << selected.port << std::endl;
        }
    }
    
    // 测试加权轮询负载均衡
    std::cout << "\n策略 3: 加权轮询负载均衡 (Weighted Round-Robin)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    {
        // 创建不同权重的实例
        std::vector<ServiceInstance> weighted_instances = {
            {"192.168.1.101", 8080, 5},  // 权重 5
            {"192.168.1.102", 8080, 2},  // 权重 2
            {"192.168.1.103", 8080, 1}   // 权重 1
        };
        
        WeightedRoundRobinLoadBalancer lb;
        std::cout << "实例权重: 101(5), 102(2), 103(1)" << std::endl;
        std::cout << "连续 8 次选择：" << std::endl;
        
        for (int i = 0; i < 8; ++i) {
            auto selected = lb.select(weighted_instances);
            std::cout << "  " << (i + 1) << ". " << selected.host << ":" << selected.port 
                      << " (权重: " << selected.weight << ")" << std::endl;
        }
    }
}

// ============================================================================
// 示例 3: 健康检测和故障转移
// ============================================================================

void example_health_checking() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  示例 3: 健康检测和故障转移                               ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // 创建服务注册中心
    ServiceRegistry registry;
    
    // 注册服务实例
    ServiceInstance instance1{"192.168.1.101", 8080, 100};
    ServiceInstance instance2{"192.168.1.102", 8080, 100};
    ServiceInstance instance3{"192.168.1.103", 8080, 100};
    
    registry.register_service("calc_service", instance1);
    registry.register_service("calc_service", instance2);
    registry.register_service("calc_service", instance3);
    
    std::cout << "初始状态: 3 个健康实例" << std::endl;
    auto instances = registry.get_instances("calc_service");
    std::cout << "✓ 健康实例数: " << instances.size() << std::endl;
    
    std::cout << "\n模拟实例 2 故障" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 模拟实例 2 变为不健康
    registry.update_health_status("calc_service", instance2, false);
    std::cout << "✓ 标记实例 2 为不健康" << std::endl;
    
    // 查询健康实例
    instances = registry.get_instances("calc_service");
    std::cout << "✓ 当前健康实例数: " << instances.size() << std::endl;
    std::cout << "  健康实例列表：" << std::endl;
    for (const auto& inst : instances) {
        std::cout << "  - " << inst.host << ":" << inst.port << std::endl;
    }
    
    std::cout << "\n模拟实例 2 恢复" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 模拟实例 2 恢复健康
    registry.update_health_status("calc_service", instance2, true);
    std::cout << "✓ 标记实例 2 为健康" << std::endl;
    
    // 再次查询
    instances = registry.get_instances("calc_service");
    std::cout << "✓ 当前健康实例数: " << instances.size() << std::endl;
}

// ============================================================================
// 示例 4: 完整的端到端 RPC 调用
// ============================================================================

void example_end_to_end_rpc() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  示例 4: 完整的端到端 RPC 调用                            ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // 创建服务注册中心
    ServiceRegistry registry;
    
    // 创建服务实例模拟器
    std::vector<std::unique_ptr<ServiceInstanceSimulator>> simulators;
    simulators.push_back(std::make_unique<ServiceInstanceSimulator>(
        "Instance-1", "192.168.1.101", 8080, 100));
    simulators.push_back(std::make_unique<ServiceInstanceSimulator>(
        "Instance-2", "192.168.1.102", 8080, 100));
    simulators.push_back(std::make_unique<ServiceInstanceSimulator>(
        "Instance-3", "192.168.1.103", 8080, 100));
    
    // 注册所有实例
    std::cout << "\n步骤 1: 注册服务实例" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    for (const auto& sim : simulators) {
        registry.register_service("calc_service", sim->instance());
    }
    std::cout << "✓ 已注册 " << simulators.size() << " 个实例\n" << std::endl;
    
    // 创建负载均衡器
    RoundRobinLoadBalancer load_balancer;
    
    // 创建客户端（使用模拟的传输函数）
    auto transport_func = [&](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        // 从注册中心获取健康实例
        auto instances = registry.get_instances("calc_service");
        if (instances.empty()) {
            std::cout << "[Client] 错误: 没有可用的服务实例" << std::endl;
            return std::nullopt;
        }
        
        // 使用负载均衡器选择实例
        auto selected = load_balancer.select(instances);
        std::cout << "[Client] 选择实例: " << selected.host << ":" << selected.port << std::endl;
        
        // 找到对应的模拟器并处理请求
        for (const auto& sim : simulators) {
            if (sim->instance() == selected) {
                return sim->handle_request(request);
            }
        }
        
        return std::nullopt;
    };
    
    RpcClient client(transport_func);
    
    std::cout << "步骤 2: 执行 RPC 调用" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 调用加法服务
    {
        std::cout << "\n调用 1: calc.add(10, 20)" << std::endl;
        ByteBuffer payload;
        auto a_bytes = serialize_int(10);
        auto b_bytes = serialize_int(20);
        payload.insert(payload.end(), a_bytes.begin(), a_bytes.end());
        payload.insert(payload.end(), b_bytes.begin(), b_bytes.end());
        
        auto resp = client.call("calc.add", payload);
        if (resp && resp->error_code == ErrorCode::Success) {
            int32_t result = deserialize_int(resp->payload);
            std::cout << "[Client] ✓ 结果: " << result << std::endl;
        }
    }
    
    // 调用乘法服务
    {
        std::cout << "\n调用 2: calc.multiply(7, 8)" << std::endl;
        ByteBuffer payload;
        auto a_bytes = serialize_int(7);
        auto b_bytes = serialize_int(8);
        payload.insert(payload.end(), a_bytes.begin(), a_bytes.end());
        payload.insert(payload.end(), b_bytes.begin(), b_bytes.end());
        
        auto resp = client.call("calc.multiply", payload);
        if (resp && resp->error_code == ErrorCode::Success) {
            int32_t result = deserialize_int(resp->payload);
            std::cout << "[Client] ✓ 结果: " << result << std::endl;
        }
    }
    
    // 调用 echo 服务
    {
        std::cout << "\n调用 3: calc.echo(\"Hello, FRPC!\")" << std::endl;
        std::string message = "Hello, FRPC!";
        ByteBuffer payload(message.begin(), message.end());
        
        auto resp = client.call("calc.echo", payload);
        if (resp && resp->error_code == ErrorCode::Success) {
            std::string result(resp->payload.begin(), resp->payload.end());
            std::cout << "[Client] ✓ 结果: " << result << std::endl;
        }
    }
}

// ============================================================================
// 示例 5: 并发场景测试
// ============================================================================

void example_concurrent_calls() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  示例 5: 并发场景测试                                     ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // 创建服务实例
    ServiceInstanceSimulator simulator("Instance-1", "192.168.1.101", 8080);
    
    // 创建客户端
    RpcClient client([&simulator](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return simulator.handle_request(request);
    });
    
    std::cout << "启动 5 个并发调用..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 创建多个线程并发调用
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&client, &success_count, i]() {
            // 准备参数
            int32_t a = i * 10;
            int32_t b = i * 5;
            
            ByteBuffer payload;
            auto a_bytes = serialize_int(a);
            auto b_bytes = serialize_int(b);
            payload.insert(payload.end(), a_bytes.begin(), a_bytes.end());
            payload.insert(payload.end(), b_bytes.begin(), b_bytes.end());
            
            // 调用服务
            auto resp = client.call("calc.add", payload);
            
            if (resp && resp->error_code == ErrorCode::Success) {
                int32_t result = deserialize_int(resp->payload);
                std::cout << "[Thread-" << i << "] " << a << " + " << b 
                          << " = " << result << " ✓" << std::endl;
                success_count++;
            } else {
                std::cout << "[Thread-" << i << "] 调用失败 ✗" << std::endl;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "\n✓ 并发测试完成: " << success_count << "/5 调用成功" << std::endl;
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                                            ║" << std::endl;
    std::cout << "║     FRPC 框架 - 完整集成示例                              ║" << std::endl;
    std::cout << "║     演示服务注册、发现、负载均衡、健康检测                ║" << std::endl;
    std::cout << "║                                                            ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        // 运行所有示例
        example_service_registration_and_discovery();
        example_load_balancing();
        example_health_checking();
        example_end_to_end_rpc();
        example_concurrent_calls();
        
        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║     所有示例执行完成！                                     ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
