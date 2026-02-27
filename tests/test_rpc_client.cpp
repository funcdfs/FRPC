/**
 * @file test_rpc_client.cpp
 * @brief RPC 客户端单元测试
 * 
 * 测试 RPC 客户端的核心功能：
 * - 远程服务调用
 * - 网络传输失败处理
 * - 响应反序列化失败处理
 * - 重试机制
 * - 统计信息收集
 */

#include <gtest/gtest.h>
#include "frpc/rpc_client.h"
#include "frpc/rpc_server.h"
#include <thread>
#include <chrono>

using namespace frpc;

/**
 * @brief RPC 客户端测试夹具
 */
class RpcClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建一个简单的服务端用于测试
        server_ = std::make_unique<RpcServer>();
        
        // 注册一个简单的 echo 服务
        server_->register_service("echo", [](const Request& req) -> Response {
            Response resp;
            resp.request_id = req.request_id;
            resp.error_code = ErrorCode::Success;
            resp.error_message = "";
            resp.payload = req.payload;  // 直接返回输入
            return resp;
        });
        
        // 注册一个加法服务
        server_->register_service("add", [](const Request& req) -> Response {
            Response resp;
            resp.request_id = req.request_id;
            
            // 假设 payload 包含两个 int32_t
            if (req.payload.size() != 8) {
                resp.error_code = ErrorCode::InvalidArgument;
                resp.error_message = "Invalid payload size";
                return resp;
            }
            
            int32_t a = *reinterpret_cast<const int32_t*>(req.payload.data());
            int32_t b = *reinterpret_cast<const int32_t*>(req.payload.data() + 4);
            int32_t result = a + b;
            
            ByteBuffer result_buffer(4);
            *reinterpret_cast<int32_t*>(result_buffer.data()) = result;
            
            resp.error_code = ErrorCode::Success;
            resp.error_message = "";
            resp.payload = result_buffer;
            return resp;
        });
    }

    void TearDown() override {
        server_.reset();
    }

    /**
     * @brief 创建一个传输函数，将请求发送到服务端
     */
    TransportFunction create_transport() {
        return [this](const ByteBuffer& request) -> std::optional<ByteBuffer> {
            return server_->handle_raw_request(request);
        };
    }

    /**
     * @brief 创建一个总是失败的传输函数
     */
    TransportFunction create_failing_transport() {
        return [](const ByteBuffer&) -> std::optional<ByteBuffer> {
            return std::nullopt;  // 模拟网络失败
        };
    }

    /**
     * @brief 创建一个返回无效数据的传输函数
     */
    TransportFunction create_invalid_response_transport() {
        return [](const ByteBuffer&) -> std::optional<ByteBuffer> {
            // 返回无效的响应数据
            return ByteBuffer{0xFF, 0xFF, 0xFF, 0xFF};
        };
    }

    std::unique_ptr<RpcServer> server_;
};

/**
 * @brief 测试远程服务调用 (需求 5.1)
 */
TEST_F(RpcClientTest, BasicRemoteCall) {
    RpcClient client(create_transport());
    
    // 调用 echo 服务
    ByteBuffer payload = {1, 2, 3, 4, 5};
    auto response = client.call("echo", payload);
    
    // 验证调用成功
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->error_code, ErrorCode::Success);
    EXPECT_EQ(response->payload, payload);
}

/**
 * @brief 测试网络传输失败处理 (需求 5.6)
 */
TEST_F(RpcClientTest, NetworkTransmissionFailure) {
    RpcClient client(create_failing_transport());
    
    // 设置重试次数为 0，避免重试
    client.set_max_retries(0);
    
    // 调用服务
    ByteBuffer payload = {1, 2, 3};
    auto response = client.call("echo", payload);
    
    // 验证调用失败
    EXPECT_FALSE(response.has_value());
    
    // 验证统计信息
    auto stats = client.get_stats();
    EXPECT_EQ(stats.total_calls, 1);
    EXPECT_EQ(stats.failed_calls, 1);
    EXPECT_EQ(stats.successful_calls, 0);
}

/**
 * @brief 测试响应反序列化失败处理 (需求 5.7)
 */
TEST_F(RpcClientTest, ResponseDeserializationFailure) {
    RpcClient client(create_invalid_response_transport());
    
    // 设置重试次数为 0
    client.set_max_retries(0);
    
    // 调用服务
    ByteBuffer payload = {1, 2, 3};
    auto response = client.call("echo", payload);
    
    // 验证调用失败
    EXPECT_FALSE(response.has_value());
    
    // 验证统计信息
    auto stats = client.get_stats();
    EXPECT_EQ(stats.total_calls, 1);
    EXPECT_EQ(stats.failed_calls, 1);
}

/**
 * @brief 测试重试机制
 */
TEST_F(RpcClientTest, RetryMechanism) {
    // 创建一个前两次失败，第三次成功的传输函数
    int attempt_count = 0;
    auto transport = [this, &attempt_count](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        attempt_count++;
        if (attempt_count < 3) {
            return std::nullopt;  // 前两次失败
        }
        return server_->handle_raw_request(request);  // 第三次成功
    };
    
    RpcClient client(transport);
    client.set_max_retries(3);
    
    // 调用服务
    ByteBuffer payload = {1, 2, 3};
    auto response = client.call("echo", payload);
    
    // 验证最终成功
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->error_code, ErrorCode::Success);
    EXPECT_EQ(attempt_count, 3);  // 应该尝试了 3 次
    
    // 验证统计信息
    auto stats = client.get_stats();
    EXPECT_EQ(stats.total_calls, 1);
    EXPECT_EQ(stats.successful_calls, 1);
    EXPECT_EQ(stats.failed_calls, 0);
}

/**
 * @brief 测试重试耗尽
 */
TEST_F(RpcClientTest, RetryExhausted) {
    RpcClient client(create_failing_transport());
    client.set_max_retries(2);
    
    // 调用服务
    ByteBuffer payload = {1, 2, 3};
    auto response = client.call("echo", payload);
    
    // 验证调用失败
    EXPECT_FALSE(response.has_value());
    
    // 验证统计信息
    auto stats = client.get_stats();
    EXPECT_EQ(stats.total_calls, 1);
    EXPECT_EQ(stats.failed_calls, 1);
}

/**
 * @brief 测试统计信息收集 (需求 11.4)
 */
TEST_F(RpcClientTest, StatisticsCollection) {
    RpcClient client(create_transport());
    
    // 执行多次调用
    ByteBuffer payload = {1, 2, 3};
    for (int i = 0; i < 5; ++i) {
        auto response = client.call("echo", payload);
        ASSERT_TRUE(response.has_value());
    }
    
    // 验证统计信息
    auto stats = client.get_stats();
    EXPECT_EQ(stats.total_calls, 5);
    EXPECT_EQ(stats.successful_calls, 5);
    EXPECT_EQ(stats.failed_calls, 0);
    EXPECT_GT(stats.avg_latency_ms, 0.0);  // 平均延迟应该大于 0
}

/**
 * @brief 测试统计信息重置
 */
TEST_F(RpcClientTest, StatisticsReset) {
    RpcClient client(create_transport());
    
    // 执行一些调用
    ByteBuffer payload = {1, 2, 3};
    client.call("echo", payload);
    client.call("echo", payload);
    
    // 验证统计信息
    auto stats1 = client.get_stats();
    EXPECT_EQ(stats1.total_calls, 2);
    
    // 重置统计信息
    client.reset_stats();
    
    // 验证统计信息已重置
    auto stats2 = client.get_stats();
    EXPECT_EQ(stats2.total_calls, 0);
    EXPECT_EQ(stats2.successful_calls, 0);
    EXPECT_EQ(stats2.failed_calls, 0);
    EXPECT_EQ(stats2.avg_latency_ms, 0.0);
}

/**
 * @brief 测试带元数据的调用
 */
TEST_F(RpcClientTest, CallWithMetadata) {
    RpcClient client(create_transport());
    
    // 准备元数据
    std::unordered_map<std::string, std::string> metadata;
    metadata["trace_id"] = "abc-123";
    metadata["user_id"] = "42";
    
    // 调用服务
    ByteBuffer payload = {1, 2, 3};
    auto response = client.call_with_metadata("echo", payload, metadata);
    
    // 验证调用成功
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->error_code, ErrorCode::Success);
}

/**
 * @brief 测试请求 ID 生成
 */
TEST_F(RpcClientTest, RequestIdGeneration) {
    RpcClient client(create_transport());
    
    // 执行多次调用
    ByteBuffer payload = {1, 2, 3};
    
    auto response1 = client.call("echo", payload);
    uint32_t id1 = client.last_request_id();
    
    auto response2 = client.call("echo", payload);
    uint32_t id2 = client.last_request_id();
    
    // 验证请求 ID 不同
    EXPECT_NE(id1, id2);
    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
}

/**
 * @brief 测试服务端错误响应
 */
TEST_F(RpcClientTest, ServerErrorResponse) {
    RpcClient client(create_transport());
    
    // 调用不存在的服务
    ByteBuffer payload = {1, 2, 3};
    auto response = client.call("non_existent_service", payload);
    
    // 验证收到错误响应
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->error_code, ErrorCode::ServiceNotFound);
    EXPECT_FALSE(response->error_message.empty());
    
    // 验证统计信息（服务端错误也算失败）
    auto stats = client.get_stats();
    EXPECT_EQ(stats.total_calls, 1);
    EXPECT_EQ(stats.failed_calls, 1);
}

/**
 * @brief 测试并发调用
 */
TEST_F(RpcClientTest, ConcurrentCalls) {
    RpcClient client(create_transport());
    
    const int num_threads = 4;
    const int calls_per_thread = 10;
    std::vector<std::thread> threads;
    
    // 启动多个线程并发调用
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&client]() {
            ByteBuffer payload = {1, 2, 3};
            for (int j = 0; j < 10; ++j) {
                auto response = client.call("echo", payload);
                EXPECT_TRUE(response.has_value());
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证统计信息
    auto stats = client.get_stats();
    EXPECT_EQ(stats.total_calls, num_threads * calls_per_thread);
    EXPECT_EQ(stats.successful_calls, num_threads * calls_per_thread);
}
