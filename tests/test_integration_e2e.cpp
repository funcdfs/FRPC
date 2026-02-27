/**
 * @file test_integration_e2e.cpp
 * @brief 端到端集成测试
 * 
 * 测试完整的客户端-服务端通信、服务发现、负载均衡、健康检测和并发场景。
 * 
 * 需求覆盖：
 * - 需求 4.7: 服务端并发处理多个请求
 * - 需求 8.4: 服务发现和负载均衡
 * - 需求 9.4: 健康检测和故障转移
 * - 需求 10.1: 负载均衡
 */

#include <gtest/gtest.h>
#include "frpc/rpc_server.h"
#include "frpc/rpc_client.h"
#include "frpc/service_registry.h"
#include "frpc/load_balancer.h"
#include "frpc/serializer.h"
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>

using namespace frpc;

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * @brief 序列化整数
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
 * @brief 反序列化整数
 */
int32_t deserialize_int(const ByteBuffer& buffer) {
    if (buffer.size() < 4) return 0;
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/**
 * @brief 加法服务处理器
 */
Response handle_add(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    if (req.payload.size() < 8) {
        resp.error_code = ErrorCode::InvalidArgument;
        resp.error_message = "Invalid payload";
        return resp;
    }
    
    ByteBuffer a_bytes(req.payload.begin(), req.payload.begin() + 4);
    ByteBuffer b_bytes(req.payload.begin() + 4, req.payload.begin() + 8);
    
    int32_t a = deserialize_int(a_bytes);
    int32_t b = deserialize_int(b_bytes);
    int32_t result = a + b;
    
    resp.error_code = ErrorCode::Success;
    resp.payload = serialize_int(result);
    
    return resp;
}

/**
 * @brief Echo 服务处理器
 */
Response handle_echo(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    resp.error_code = ErrorCode::Success;
    resp.payload = req.payload;
    return resp;
}

// ============================================================================
// 测试 1: 完整的客户端-服务端通信
// ============================================================================

TEST(IntegrationE2ETest, ClientServerCommunication) {
    // 创建服务端
    RpcServer server;
    server.register_service("add", handle_add);
    server.register_service("echo", handle_echo);
    
    // 创建客户端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 测试加法服务
    {
        ByteBuffer payload;
        auto a_bytes = serialize_int(10);
        auto b_bytes = serialize_int(20);
        payload.insert(payload.end(), a_bytes.begin(), a_bytes.end());
        payload.insert(payload.end(), b_bytes.begin(), b_bytes.end());
        
        auto resp = client.call("add", payload);
        
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(resp->error_code, ErrorCode::Success);
        
        int32_t result = deserialize_int(resp->payload);
        EXPECT_EQ(result, 30);
    }
    
    // 测试 echo 服务
    {
        std::string message = "Hello, FRPC!";
        ByteBuffer payload(message.begin(), message.end());
        
        auto resp = client.call("echo", payload);
        
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(resp->error_code, ErrorCode::Success);
        
        std::string result(resp->payload.begin(), resp->payload.end());
        EXPECT_EQ(result, message);
    }
}

// ============================================================================
// 测试 2: 服务发现和负载均衡
// ============================================================================

TEST(IntegrationE2ETest, ServiceDiscoveryAndLoadBalancing) {
    // 创建服务注册中心
    ServiceRegistry registry;
    
    // 注册多个服务实例
    ServiceInstance instance1{"192.168.1.101", 8080, 100};
    ServiceInstance instance2{"192.168.1.102", 8080, 100};
    ServiceInstance instance3{"192.168.1.103", 8080, 100};
    
    registry.register_service("calc_service", instance1);
    registry.register_service("calc_service", instance2);
    registry.register_service("calc_service", instance3);
    
    // 查询服务实例
    auto instances = registry.get_instances("calc_service");
    EXPECT_EQ(instances.size(), 3);
    
    // 测试轮询负载均衡
    RoundRobinLoadBalancer lb;
    
    // 连续选择应该按顺序返回实例
    auto selected1 = lb.select(instances);
    auto selected2 = lb.select(instances);
    auto selected3 = lb.select(instances);
    auto selected4 = lb.select(instances);  // 应该循环回到第一个
    
    // 验证选择的实例不同
    EXPECT_NE(selected1, selected2);
    EXPECT_NE(selected2, selected3);
    
    // 验证循环
    EXPECT_EQ(selected1, selected4);
}

// ============================================================================
// 测试 3: 健康检测和故障转移
// ============================================================================

TEST(IntegrationE2ETest, HealthCheckingAndFailover) {
    // 创建服务注册中心
    ServiceRegistry registry;
    
    // 注册服务实例
    ServiceInstance instance1{"192.168.1.101", 8080, 100};
    ServiceInstance instance2{"192.168.1.102", 8080, 100};
    ServiceInstance instance3{"192.168.1.103", 8080, 100};
    
    registry.register_service("calc_service", instance1);
    registry.register_service("calc_service", instance2);
    registry.register_service("calc_service", instance3);
    
    // 初始状态：3 个健康实例
    auto instances = registry.get_instances("calc_service");
    EXPECT_EQ(instances.size(), 3);
    
    // 模拟实例 2 故障
    registry.update_health_status("calc_service", instance2, false);
    
    // 查询健康实例：应该只有 2 个
    instances = registry.get_instances("calc_service");
    EXPECT_EQ(instances.size(), 2);
    
    // 验证不健康的实例不在列表中
    bool found_unhealthy = false;
    for (const auto& inst : instances) {
        if (inst == instance2) {
            found_unhealthy = true;
            break;
        }
    }
    EXPECT_FALSE(found_unhealthy);
    
    // 模拟实例 2 恢复
    registry.update_health_status("calc_service", instance2, true);
    
    // 查询健康实例：应该恢复到 3 个
    instances = registry.get_instances("calc_service");
    EXPECT_EQ(instances.size(), 3);
}

// ============================================================================
// 测试 4: 并发场景
// ============================================================================

TEST(IntegrationE2ETest, ConcurrentScenario) {
    // 创建服务端
    RpcServer server;
    server.register_service("add", handle_add);
    
    // 创建客户端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 并发调用
    const int num_threads = 10;
    const int calls_per_thread = 10;
    std::atomic<int> success_count{0};
    std::atomic<int> total_sum{0};
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&client, &success_count, &total_sum, i, calls_per_thread]() {
            for (int j = 0; j < calls_per_thread; ++j) {
                int32_t a = i * 10 + j;
                int32_t b = j * 2;
                
                ByteBuffer payload;
                auto a_bytes = serialize_int(a);
                auto b_bytes = serialize_int(b);
                payload.insert(payload.end(), a_bytes.begin(), a_bytes.end());
                payload.insert(payload.end(), b_bytes.begin(), b_bytes.end());
                
                auto resp = client.call("add", payload);
                
                if (resp && resp->error_code == ErrorCode::Success) {
                    int32_t result = deserialize_int(resp->payload);
                    EXPECT_EQ(result, a + b);
                    success_count++;
                    total_sum += result;
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证所有调用都成功
    EXPECT_EQ(success_count, num_threads * calls_per_thread);
    
    // 验证总和正确
    int expected_sum = 0;
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < calls_per_thread; ++j) {
            expected_sum += (i * 10 + j) + (j * 2);
        }
    }
    EXPECT_EQ(total_sum, expected_sum);
}

// ============================================================================
// 测试 5: 服务变更通知
// ============================================================================

TEST(IntegrationE2ETest, ServiceChangeNotification) {
    ServiceRegistry registry;
    
    std::atomic<int> notification_count{0};
    std::vector<ServiceInstance> last_instances;
    
    // 订阅服务变更
    registry.subscribe("calc_service", [&](const std::vector<ServiceInstance>& instances) {
        notification_count++;
        last_instances = instances;
    });
    
    // 注册第一个实例 - 应该触发通知
    ServiceInstance instance1{"192.168.1.101", 8080, 100};
    registry.register_service("calc_service", instance1);
    
    EXPECT_EQ(notification_count, 1);
    EXPECT_EQ(last_instances.size(), 1);
    
    // 注册第二个实例 - 应该触发通知
    ServiceInstance instance2{"192.168.1.102", 8080, 100};
    registry.register_service("calc_service", instance2);
    
    EXPECT_EQ(notification_count, 2);
    EXPECT_EQ(last_instances.size(), 2);
    
    // 注销第一个实例 - 应该触发通知
    registry.unregister_service("calc_service", instance1);
    
    EXPECT_EQ(notification_count, 3);
    EXPECT_EQ(last_instances.size(), 1);
}

// ============================================================================
// 测试 6: 负载均衡策略切换
// ============================================================================

TEST(IntegrationE2ETest, LoadBalancingStrategySwitching) {
    std::vector<ServiceInstance> instances = {
        {"192.168.1.101", 8080, 100},
        {"192.168.1.102", 8080, 100},
        {"192.168.1.103", 8080, 100}
    };
    
    // 测试轮询策略
    {
        RoundRobinLoadBalancer lb;
        auto selected1 = lb.select(instances);
        auto selected2 = lb.select(instances);
        auto selected3 = lb.select(instances);
        
        // 应该选择不同的实例
        EXPECT_NE(selected1, selected2);
        EXPECT_NE(selected2, selected3);
    }
    
    // 测试随机策略
    {
        RandomLoadBalancer lb;
        
        // 多次选择，统计每个实例被选中的次数
        std::map<std::string, int> counts;
        const int num_selections = 300;
        
        for (int i = 0; i < num_selections; ++i) {
            auto selected = lb.select(instances);
            counts[selected.host]++;
        }
        
        // 每个实例应该被选中大约 100 次（允许一定偏差）
        for (const auto& [host, count] : counts) {
            EXPECT_GT(count, 50);   // 至少 50 次
            EXPECT_LT(count, 150);  // 最多 150 次
        }
    }
    
    // 测试加权轮询策略
    {
        std::vector<ServiceInstance> weighted_instances = {
            {"192.168.1.101", 8080, 5},  // 权重 5
            {"192.168.1.102", 8080, 2},  // 权重 2
            {"192.168.1.103", 8080, 1}   // 权重 1
        };
        
        WeightedRoundRobinLoadBalancer lb;
        
        // 选择 8 次（权重总和）
        std::map<std::string, int> counts;
        for (int i = 0; i < 8; ++i) {
            auto selected = lb.select(weighted_instances);
            counts[selected.host]++;
        }
        
        // 验证权重分配
        EXPECT_EQ(counts["192.168.1.101"], 5);
        EXPECT_EQ(counts["192.168.1.102"], 2);
        EXPECT_EQ(counts["192.168.1.103"], 1);
    }
    
    // Note: LeastConnectionLoadBalancer test is skipped due to ConnectionPool dependency
}

// ============================================================================
// 测试 7: 错误处理和恢复
// ============================================================================

TEST(IntegrationE2ETest, ErrorHandlingAndRecovery) {
    // 创建服务端
    RpcServer server;
    server.register_service("add", handle_add);
    
    // 创建客户端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 测试调用不存在的服务
    {
        auto resp = client.call("non_existent_service", {});
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(resp->error_code, ErrorCode::ServiceNotFound);
        EXPECT_FALSE(resp->error_message.empty());
    }
    
    // 测试无效参数
    {
        ByteBuffer invalid_payload = {1, 2, 3};  // 太短，无法解析为两个整数
        auto resp = client.call("add", invalid_payload);
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(resp->error_code, ErrorCode::InvalidArgument);
    }
    
    // 测试正常调用仍然工作
    {
        ByteBuffer payload;
        auto a_bytes = serialize_int(5);
        auto b_bytes = serialize_int(7);
        payload.insert(payload.end(), a_bytes.begin(), a_bytes.end());
        payload.insert(payload.end(), b_bytes.begin(), b_bytes.end());
        
        auto resp = client.call("add", payload);
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(resp->error_code, ErrorCode::Success);
        
        int32_t result = deserialize_int(resp->payload);
        EXPECT_EQ(result, 12);
    }
}

// ============================================================================
// 测试 8: 元数据传递
// ============================================================================

TEST(IntegrationE2ETest, MetadataTransmission) {
    // 创建服务端
    RpcServer server;
    server.register_service("echo", handle_echo);
    
    // 创建客户端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 准备元数据
    std::unordered_map<std::string, std::string> metadata;
    metadata["trace_id"] = "trace-12345";
    metadata["user_id"] = "user-67890";
    metadata["client_version"] = "1.0.0";
    
    // 调用服务并传递元数据
    std::string message = "Hello with metadata";
    ByteBuffer payload(message.begin(), message.end());
    
    auto resp = client.call_with_metadata("echo", payload, metadata);
    
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->error_code, ErrorCode::Success);
    
    std::string result(resp->payload.begin(), resp->payload.end());
    EXPECT_EQ(result, message);
    
    // 验证元数据被传递（响应中应该包含相同的元数据）
    EXPECT_EQ(resp->metadata.size(), metadata.size());
    for (const auto& [key, value] : metadata) {
        EXPECT_EQ(resp->metadata.at(key), value);
    }
}

