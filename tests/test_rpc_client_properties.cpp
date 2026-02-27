/**
 * @file test_rpc_client_properties.cpp
 * @brief RPC 客户端属性测试
 * 
 * 使用 RapidCheck 进行基于属性的测试，验证 RPC 客户端的正确性属性。
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "frpc/rpc_client.h"
#include "frpc/rpc_server.h"
#include <thread>
#include <chrono>

using namespace frpc;

/**
 * @brief 生成随机的 ByteBuffer
 */
namespace rc {
template<>
struct Arbitrary<ByteBuffer> {
    static Gen<ByteBuffer> arbitrary() {
        return gen::container<ByteBuffer>(gen::arbitrary<uint8_t>());
    }
};
}

/**
 * @brief 创建一个 echo 服务端
 */
std::unique_ptr<RpcServer> create_echo_server() {
    auto server = std::make_unique<RpcServer>();
    
    // 注册 echo 服务：返回输入的 payload
    server->register_service("echo", [](const Request& req) -> Response {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = req.payload;
        return resp;
    });
    
    // 注册 reverse 服务：反转 payload
    server->register_service("reverse", [](const Request& req) -> Response {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = req.payload;
        std::reverse(resp.payload.begin(), resp.payload.end());
        return resp;
    });
    
    // 注册 length 服务：返回 payload 的长度
    server->register_service("length", [](const Request& req) -> Response {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        
        // 将长度编码为 4 字节
        uint32_t length = static_cast<uint32_t>(req.payload.size());
        resp.payload.resize(4);
        *reinterpret_cast<uint32_t*>(resp.payload.data()) = length;
        
        return resp;
    });
    
    return server;
}

/**
 * @brief 创建传输函数
 */
TransportFunction create_transport(RpcServer* server) {
    return [server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server->handle_raw_request(request);
    };
}

// Feature: frpc-framework, Property 8: RPC 客户端端到端调用
/**
 * @brief Property 8: RPC 客户端端到端调用
 * 
 * 验证：对于任意有效的服务调用（服务名称和参数），RPC Client 应该完成
 * 序列化请求、发送请求、接收响应、反序列化响应的完整流程，并返回正确的结果。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5**
 */
RC_GTEST_PROP(RpcClientPropertyTest, EndToEndCall, (const ByteBuffer& payload)) {
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 调用 echo 服务
    auto response = client.call("echo", payload);
    
    // 验证调用成功
    RC_ASSERT(response.has_value());
    RC_ASSERT(response->error_code == ErrorCode::Success);
    
    // 验证返回的 payload 与输入相同
    RC_ASSERT(response->payload == payload);
    
    // 验证请求 ID 被正确设置
    RC_ASSERT(response->request_id == client.last_request_id());
}

// Feature: frpc-framework, Property 8: RPC 客户端端到端调用（reverse 服务）
/**
 * @brief Property 8 变体: RPC 客户端端到端调用 - reverse 服务
 * 
 * 验证：RPC 客户端能够正确调用 reverse 服务，返回反转后的 payload。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5**
 */
RC_GTEST_PROP(RpcClientPropertyTest, EndToEndCallReverse, (const ByteBuffer& payload)) {
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 调用 reverse 服务
    auto response = client.call("reverse", payload);
    
    // 验证调用成功
    RC_ASSERT(response.has_value());
    RC_ASSERT(response->error_code == ErrorCode::Success);
    
    // 验证返回的 payload 是输入的反转
    ByteBuffer expected = payload;
    std::reverse(expected.begin(), expected.end());
    RC_ASSERT(response->payload == expected);
}

// Feature: frpc-framework, Property 8: RPC 客户端端到端调用（length 服务）
/**
 * @brief Property 8 变体: RPC 客户端端到端调用 - length 服务
 * 
 * 验证：RPC 客户端能够正确调用 length 服务，返回 payload 的长度。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5**
 */
RC_GTEST_PROP(RpcClientPropertyTest, EndToEndCallLength, (const ByteBuffer& payload)) {
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 调用 length 服务
    auto response = client.call("length", payload);
    
    // 验证调用成功
    RC_ASSERT(response.has_value());
    RC_ASSERT(response->error_code == ErrorCode::Success);
    
    // 验证返回的长度正确
    RC_ASSERT(response->payload.size() == 4);
    uint32_t returned_length = *reinterpret_cast<const uint32_t*>(response->payload.data());
    RC_ASSERT(returned_length == payload.size());
}

// Feature: frpc-framework, Property 8: 序列化和反序列化的正确性
/**
 * @brief Property 8 变体: 序列化和反序列化的正确性
 * 
 * 验证：对于任意 payload，经过序列化、网络传输、反序列化后，
 * 数据保持不变（往返属性）。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5**
 */
RC_GTEST_PROP(RpcClientPropertyTest, SerializationRoundTrip, (const ByteBuffer& payload)) {
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 多次调用，验证每次都正确
    for (int i = 0; i < 3; ++i) {
        auto response = client.call("echo", payload);
        
        RC_ASSERT(response.has_value());
        RC_ASSERT(response->error_code == ErrorCode::Success);
        RC_ASSERT(response->payload == payload);
    }
}

// Feature: frpc-framework, Property 8: 带元数据的端到端调用
/**
 * @brief Property 8 变体: 带元数据的端到端调用
 * 
 * 验证：RPC 客户端能够正确处理带元数据的调用。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5**
 */
RC_GTEST_PROP(RpcClientPropertyTest, EndToEndCallWithMetadata, 
              (const ByteBuffer& payload, const std::string& trace_id)) {
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 准备元数据
    std::unordered_map<std::string, std::string> metadata;
    metadata["trace_id"] = trace_id;
    
    // 调用服务
    auto response = client.call_with_metadata("echo", payload, metadata);
    
    // 验证调用成功
    RC_ASSERT(response.has_value());
    RC_ASSERT(response->error_code == ErrorCode::Success);
    RC_ASSERT(response->payload == payload);
}

// Feature: frpc-framework, Property 8: 并发调用的正确性
/**
 * @brief Property 8 变体: 并发调用的正确性
 * 
 * 验证：多个并发的 RPC 调用都能正确完成，且结果互不干扰。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5**
 */
RC_GTEST_PROP(RpcClientPropertyTest, ConcurrentCalls, 
              (const std::vector<ByteBuffer>& payloads)) {
    RC_PRE(!payloads.empty() && payloads.size() <= 10);  // 限制并发数
    
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 存储结果
    std::vector<std::optional<Response>> responses(payloads.size());
    std::vector<std::thread> threads;
    
    // 启动多个线程并发调用
    for (size_t i = 0; i < payloads.size(); ++i) {
        threads.emplace_back([&client, &payloads, &responses, i]() {
            responses[i] = client.call("echo", payloads[i]);
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有调用都成功，且结果正确
    for (size_t i = 0; i < payloads.size(); ++i) {
        RC_ASSERT(responses[i].has_value());
        RC_ASSERT(responses[i]->error_code == ErrorCode::Success);
        RC_ASSERT(responses[i]->payload == payloads[i]);
    }
}

// Feature: frpc-framework, Property 8: 错误响应的正确处理
/**
 * @brief Property 8 变体: 错误响应的正确处理
 * 
 * 验证：当服务端返回错误响应时，客户端能够正确接收和处理。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5**
 */
RC_GTEST_PROP(RpcClientPropertyTest, ErrorResponseHandling, (const ByteBuffer& payload)) {
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 调用不存在的服务
    auto response = client.call("non_existent_service", payload);
    
    // 验证收到错误响应
    RC_ASSERT(response.has_value());
    RC_ASSERT(response->error_code == ErrorCode::ServiceNotFound);
    RC_ASSERT(!response->error_message.empty());
}

// Feature: frpc-framework, Property 8: 统计信息的正确性
/**
 * @brief Property 8 变体: 统计信息的正确性
 * 
 * 验证：客户端正确收集调用统计信息。
 * 
 * **Validates: Requirements 5.2, 5.3, 5.4, 5.5, 11.4**
 */
RC_GTEST_PROP(RpcClientPropertyTest, StatisticsCorrectness, 
              (const std::vector<ByteBuffer>& payloads)) {
    RC_PRE(!payloads.empty() && payloads.size() <= 20);
    
    // 创建服务端和客户端
    auto server = create_echo_server();
    RpcClient client(create_transport(server.get()));
    
    // 执行多次调用
    size_t success_count = 0;
    for (const auto& payload : payloads) {
        auto response = client.call("echo", payload);
        if (response && response->error_code == ErrorCode::Success) {
            success_count++;
        }
    }
    
    // 验证统计信息
    auto stats = client.get_stats();
    RC_ASSERT(stats.total_calls == payloads.size());
    RC_ASSERT(stats.successful_calls == success_count);
    RC_ASSERT(stats.avg_latency_ms >= 0.0);
}
