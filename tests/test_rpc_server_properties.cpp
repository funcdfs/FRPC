/**
 * @file test_rpc_server_properties.cpp
 * @brief RPC 服务端属性测试
 * 
 * 使用 RapidCheck 进行基于属性的测试，验证 RPC 服务端的核心属性。
 * 
 * 测试的属性包括：
 * - Property 5: RPC 请求路由正确性
 * - Property 6: RPC 服务端响应序列化
 * - Property 7: RPC 服务端并发处理
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "frpc/rpc_server.h"
#include "frpc/serializer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <set>
#include <algorithm>

using namespace frpc;

// ============================================================================
// RapidCheck Generators
// ============================================================================

namespace rc {

/**
 * @brief Request 生成器
 * 
 * 生成随机的 RPC 请求，用于属性测试。
 */
template<>
struct Arbitrary<Request> {
    static Gen<Request> arbitrary() {
        return gen::build<Request>(
            gen::set(&Request::request_id, gen::inRange<uint32_t>(1, 1000000)),
            gen::set(&Request::service_name, 
                     gen::element<std::string>("add", "multiply", "echo", 
                                               "user_service", "order_service")),
            gen::set(&Request::payload, 
                     gen::container<std::vector<uint8_t>>(gen::arbitrary<uint8_t>())),
            gen::set(&Request::timeout, 
                     gen::just(std::chrono::milliseconds(5000)))
        );
    }
};

/**
 * @brief 服务名称生成器
 * 
 * 生成随机的服务名称。
 */
Gen<std::string> genServiceName() {
    return gen::element<std::string>(
        "add",
        "subtract",
        "multiply",
        "divide",
        "echo",
        "reverse",
        "uppercase",
        "lowercase"
    );
}

/**
 * @brief 生成唯一的服务名称列表
 */
Gen<std::vector<std::string>> genUniqueServiceNames() {
    return gen::container<std::vector<std::string>>(genServiceName())
        .as("unique service names");
}

}  // namespace rc

// ============================================================================
// Property-Based Tests
// ============================================================================

/**
 * Feature: frpc-framework, Property 5: RPC 请求路由正确性
 * 
 * **Validates: Requirements 4.3**
 * 
 * 属性：FOR ANY 已注册的服务和有效的 RPC 请求，
 * 当请求的服务名称匹配已注册服务时，
 * RPC Server 应该将请求路由到对应的处理函数，
 * 且处理函数应该被正确调用。
 * 
 * 测试策略：
 * 1. 生成随机的服务名称
 * 2. 注册服务处理函数（在响应中标记服务名称）
 * 3. 生成请求该服务的 RPC 请求
 * 4. 处理请求
 * 5. 验证响应来自正确的处理函数
 */
RC_GTEST_PROP(RpcServerPropertyTest, RequestRoutingCorrectness,
              (const std::string& service_name, uint32_t request_id)) {
    // 前置条件：服务名称非空
    RC_PRE(!service_name.empty());
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 注册服务处理函数
    // 处理函数在响应的 payload 中返回服务名称，以便验证路由正确性
    bool registered = server.register_service(service_name, [service_name](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        // 将服务名称编码到 payload 中
        resp.payload.assign(service_name.begin(), service_name.end());
        return resp;
    });
    
    RC_ASSERT(registered);
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {1, 2, 3};
    request.timeout = std::chrono::milliseconds(5000);
    
    // 处理请求
    Response response = server.handle_request(request);
    
    // 验证：响应的 request_id 匹配
    RC_ASSERT(response.request_id == request_id);
    
    // 验证：请求成功
    RC_ASSERT(response.error_code == ErrorCode::Success);
    
    // 验证：响应来自正确的处理函数
    // payload 应该包含服务名称
    std::string response_service_name(response.payload.begin(), response.payload.end());
    RC_ASSERT(response_service_name == service_name);
}

/**
 * Feature: frpc-framework, Property 5: RPC 请求路由正确性 - 多服务路由
 * 
 * **Validates: Requirements 4.3**
 * 
 * 属性：注册多个服务后，每个请求应该被路由到正确的处理函数。
 * 
 * 测试策略：
 * 1. 注册多个不同的服务
 * 2. 为每个服务生成请求
 * 3. 验证每个请求都被路由到正确的处理函数
 */
RC_GTEST_PROP(RpcServerPropertyTest, MultipleServiceRouting,
              (const std::vector<std::string>& service_names)) {
    // 前置条件：至少有 2 个服务，最多 10 个
    RC_PRE(service_names.size() >= 2u && service_names.size() <= 10u);
    
    // 去重服务名称
    std::vector<std::string> unique_services;
    for (const auto& name : service_names) {
        if (!name.empty() && 
            std::find(unique_services.begin(), unique_services.end(), name) == unique_services.end()) {
            unique_services.push_back(name);
        }
    }
    
    RC_PRE(unique_services.size() >= 2u);
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 为每个服务分配一个唯一的标识符
    std::unordered_map<std::string, uint8_t> service_ids;
    uint8_t id = 1;
    
    // 注册所有服务
    for (const auto& service_name : unique_services) {
        service_ids[service_name] = id;
        
        server.register_service(service_name, [id](const Request& req) {
            Response resp;
            resp.request_id = req.request_id;
            resp.error_code = ErrorCode::Success;
            resp.error_message = "";
            // 将服务 ID 编码到 payload 中
            resp.payload = {id};
            return resp;
        });
        
        id++;
    }
    
    // 为每个服务创建请求并验证路由
    for (const auto& service_name : unique_services) {
        Request request;
        request.request_id = service_ids[service_name];
        request.service_name = service_name;
        request.payload = {};
        request.timeout = std::chrono::milliseconds(5000);
        
        // 处理请求
        Response response = server.handle_request(request);
        
        // 验证：请求成功
        RC_ASSERT(response.error_code == ErrorCode::Success);
        
        // 验证：响应来自正确的处理函数
        RC_ASSERT(!response.payload.empty());
        RC_ASSERT(response.payload[0] == service_ids[service_name]);
    }
}

/**
 * Feature: frpc-framework, Property 5: RPC 请求路由正确性 - 处理函数参数传递
 * 
 * **Validates: Requirements 4.3**
 * 
 * 属性：请求的 payload 应该被正确传递给处理函数。
 * 
 * 测试策略：
 * 1. 注册一个回显服务（返回请求的 payload）
 * 2. 生成随机的 payload
 * 3. 验证响应的 payload 与请求的 payload 相同
 */
RC_GTEST_PROP(RpcServerPropertyTest, HandlerParameterPassing,
              (const std::string& service_name, 
               const std::vector<uint8_t>& payload)) {
    RC_PRE(!service_name.empty());
    RC_PRE(payload.size() <= 1000u);  // 限制 payload 大小
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 注册回显服务
    server.register_service(service_name, [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        // 回显 payload
        resp.payload = req.payload;
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = 123;
    request.service_name = service_name;
    request.payload = payload;
    request.timeout = std::chrono::milliseconds(5000);
    
    // 处理请求
    Response response = server.handle_request(request);
    
    // 验证：请求成功
    RC_ASSERT(response.error_code == ErrorCode::Success);
    
    // 验证：payload 被正确传递
    RC_ASSERT(response.payload == payload);
}

/**
 * Feature: frpc-framework, Property 5: RPC 请求路由正确性 - Request ID 保持
 * 
 * **Validates: Requirements 4.3**
 * 
 * 属性：响应的 request_id 应该与请求的 request_id 相同。
 * 
 * 测试策略：
 * 1. 注册服务
 * 2. 使用各种 request_id 发送请求
 * 3. 验证响应的 request_id 匹配
 */
RC_GTEST_PROP(RpcServerPropertyTest, RequestIdPreservation,
              (const std::string& service_name, uint32_t request_id)) {
    RC_PRE(!service_name.empty());
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 注册服务
    server.register_service(service_name, [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = {};
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {};
    request.timeout = std::chrono::milliseconds(5000);
    
    // 处理请求
    Response response = server.handle_request(request);
    
    // 验证：request_id 保持不变
    RC_ASSERT(response.request_id == request_id);
}

/**
 * Feature: frpc-framework, Property 5: RPC 请求路由正确性 - 未注册服务
 * 
 * **Validates: Requirements 4.3, 4.6**
 * 
 * 属性：请求未注册的服务应该返回 ServiceNotFound 错误。
 * 
 * 测试策略：
 * 1. 不注册任何服务
 * 2. 发送请求
 * 3. 验证返回 ServiceNotFound 错误
 */
RC_GTEST_PROP(RpcServerPropertyTest, UnregisteredServiceError,
              (const std::string& service_name, uint32_t request_id)) {
    RC_PRE(!service_name.empty());
    
    // 创建 RPC 服务器（不注册任何服务）
    RpcServer server;
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {};
    request.timeout = std::chrono::milliseconds(5000);
    
    // 处理请求
    Response response = server.handle_request(request);
    
    // 验证：返回 ServiceNotFound 错误
    RC_ASSERT(response.error_code == ErrorCode::ServiceNotFound);
    
    // 验证：request_id 保持不变
    RC_ASSERT(response.request_id == request_id);
    
    // 验证：错误消息非空
    RC_ASSERT(!response.error_message.empty());
}

/**
 * Feature: frpc-framework, Property 5: RPC 请求路由正确性 - 序列化往返
 * 
 * **Validates: Requirements 4.2, 4.3, 4.4**
 * 
 * 属性：通过序列化的请求应该被正确路由和处理。
 * 
 * 测试策略：
 * 1. 注册服务
 * 2. 创建请求并序列化
 * 3. 使用 handle_raw_request 处理
 * 4. 反序列化响应并验证
 */
RC_GTEST_PROP(RpcServerPropertyTest, SerializedRequestRouting,
              (const std::string& service_name, uint32_t request_id,
               const std::vector<uint8_t>& payload)) {
    RC_PRE(!service_name.empty());
    RC_PRE(payload.size() <= 1000u);
    
    // 创建 RPC 服务器和序列化器
    RpcServer server;
    BinarySerializer serializer;
    
    // 注册回显服务
    server.register_service(service_name, [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = req.payload;
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = payload;
    request.timeout = std::chrono::milliseconds(5000);
    
    // 序列化请求
    auto request_bytes = serializer.serialize(request);
    
    // 处理序列化的请求
    auto response_bytes = server.handle_raw_request(request_bytes);
    
    // 反序列化响应
    auto response = serializer.deserialize_response(response_bytes);
    
    // 验证：请求成功
    RC_ASSERT(response.error_code == ErrorCode::Success);
    
    // 验证：request_id 匹配
    RC_ASSERT(response.request_id == request_id);
    
    // 验证：payload 被正确传递
    RC_ASSERT(response.payload == payload);
}

/**
 * Feature: frpc-framework, Property 5: RPC 请求路由正确性 - 连续请求
 * 
 * **Validates: Requirements 4.3**
 * 
 * 属性：连续处理多个请求，每个请求都应该被正确路由。
 * 
 * 测试策略：
 * 1. 注册服务
 * 2. 连续发送多个请求
 * 3. 验证每个响应都正确
 */
RC_GTEST_PROP(RpcServerPropertyTest, ConsecutiveRequests,
              (const std::string& service_name)) {
    RC_PRE(!service_name.empty());
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 注册服务（返回 request_id 作为 payload）
    server.register_service(service_name, [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        // 将 request_id 编码到 payload
        resp.payload.resize(4);
        resp.payload[0] = (req.request_id >> 24) & 0xFF;
        resp.payload[1] = (req.request_id >> 16) & 0xFF;
        resp.payload[2] = (req.request_id >> 8) & 0xFF;
        resp.payload[3] = req.request_id & 0xFF;
        return resp;
    });
    
    // 生成请求数量
    auto num_requests = *rc::gen::inRange(5, 21);
    
    // 连续发送请求
    for (int i = 0; i < num_requests; ++i) {
        Request request;
        request.request_id = static_cast<uint32_t>(i);
        request.service_name = service_name;
        request.payload = {};
        request.timeout = std::chrono::milliseconds(5000);
        
        // 处理请求
        Response response = server.handle_request(request);
        
        // 验证：请求成功
        RC_ASSERT(response.error_code == ErrorCode::Success);
        
        // 验证：request_id 匹配
        RC_ASSERT(response.request_id == static_cast<uint32_t>(i));
        
        // 验证：payload 包含正确的 request_id
        RC_ASSERT(response.payload.size() == 4u);
        uint32_t payload_id = (static_cast<uint32_t>(response.payload[0]) << 24) |
                              (static_cast<uint32_t>(response.payload[1]) << 16) |
                              (static_cast<uint32_t>(response.payload[2]) << 8) |
                              static_cast<uint32_t>(response.payload[3]);
        RC_ASSERT(payload_id == static_cast<uint32_t>(i));
    }
}

// ============================================================================
// Property 6: RPC 服务端响应序列化
// ============================================================================

/**
 * Feature: frpc-framework, Property 6: RPC 服务端响应序列化
 * 
 * **Validates: Requirements 4.4**
 * 
 * 属性：FOR ANY 处理函数返回的结果，
 * RPC Server 应该将结果序列化为 Response 对象并通过网络发送，
 * 且发送的数据应该能够被客户端正确反序列化。
 * 
 * 测试策略：
 * 1. 注册服务处理函数，返回各种类型的响应数据
 * 2. 发送请求并获取序列化的响应字节流
 * 3. 反序列化响应字节流
 * 4. 验证反序列化后的响应与原始响应等价
 * 5. 验证序列化-反序列化往返保持数据完整性
 */
RC_GTEST_PROP(RpcServerPropertyTest, ResponseSerializationRoundTrip,
              (const std::string& service_name, 
               uint32_t request_id,
               const std::vector<uint8_t>& response_payload,
               const std::string& metadata_key,
               const std::string& metadata_value)) {
    // 前置条件：服务名称非空，payload 大小合理
    RC_PRE(!service_name.empty());
    RC_PRE(response_payload.size() <= 10000u);  // 限制 payload 大小
    RC_PRE(metadata_key.size() <= 100u);
    RC_PRE(metadata_value.size() <= 100u);
    
    // 创建 RPC 服务器和序列化器
    RpcServer server;
    BinarySerializer serializer;
    
    // 注册服务处理函数
    // 处理函数返回包含指定 payload 和 metadata 的响应
    server.register_service(service_name, 
        [response_payload, metadata_key, metadata_value](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = response_payload;
        
        // 添加 metadata（如果 key 非空）
        if (!metadata_key.empty()) {
            resp.metadata[metadata_key] = metadata_value;
        }
        
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {1, 2, 3};  // 请求 payload 不重要
    request.timeout = std::chrono::milliseconds(5000);
    
    // 序列化请求
    auto request_bytes = serializer.serialize(request);
    
    // 处理请求，获取序列化的响应
    auto response_bytes = server.handle_raw_request(request_bytes);
    
    // 验证：响应字节流非空
    RC_ASSERT(!response_bytes.empty());
    
    // 反序列化响应
    Response deserialized_response = serializer.deserialize_response(response_bytes);
    
    // 验证：request_id 匹配
    RC_ASSERT(deserialized_response.request_id == request_id);
    
    // 验证：错误码为成功
    RC_ASSERT(deserialized_response.error_code == ErrorCode::Success);
    
    // 验证：错误消息为空
    RC_ASSERT(deserialized_response.error_message.empty());
    
    // 验证：payload 完整保留
    RC_ASSERT(deserialized_response.payload == response_payload);
    
    // 验证：metadata 完整保留
    if (!metadata_key.empty()) {
        RC_ASSERT(deserialized_response.metadata.count(metadata_key) > 0);
        RC_ASSERT(deserialized_response.metadata.at(metadata_key) == metadata_value);
    }
}

/**
 * Feature: frpc-framework, Property 6: RPC 服务端响应序列化 - 错误响应
 * 
 * **Validates: Requirements 4.4**
 * 
 * 属性：错误响应也应该被正确序列化和反序列化。
 * 
 * 测试策略：
 * 1. 注册服务处理函数，返回错误响应
 * 2. 验证错误响应被正确序列化
 * 3. 验证反序列化后的错误信息完整
 */
RC_GTEST_PROP(RpcServerPropertyTest, ErrorResponseSerialization,
              (const std::string& service_name,
               uint32_t request_id,
               const std::string& error_message)) {
    RC_PRE(!service_name.empty());
    RC_PRE(error_message.size() <= 1000u);
    
    // 创建 RPC 服务器和序列化器
    RpcServer server;
    BinarySerializer serializer;
    
    // 注册服务处理函数，返回错误响应
    server.register_service(service_name, 
        [error_message](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::ServiceException;
        resp.error_message = error_message;
        resp.payload = {};  // 错误响应通常没有 payload
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {};
    request.timeout = std::chrono::milliseconds(5000);
    
    // 序列化请求
    auto request_bytes = serializer.serialize(request);
    
    // 处理请求，获取序列化的响应
    auto response_bytes = server.handle_raw_request(request_bytes);
    
    // 反序列化响应
    Response deserialized_response = serializer.deserialize_response(response_bytes);
    
    // 验证：request_id 匹配
    RC_ASSERT(deserialized_response.request_id == request_id);
    
    // 验证：错误码为 ServiceException
    RC_ASSERT(deserialized_response.error_code == ErrorCode::ServiceException);
    
    // 验证：错误消息完整保留
    RC_ASSERT(deserialized_response.error_message == error_message);
    
    // 验证：payload 为空
    RC_ASSERT(deserialized_response.payload.empty());
}

/**
 * Feature: frpc-framework, Property 6: RPC 服务端响应序列化 - 大 Payload
 * 
 * **Validates: Requirements 4.4**
 * 
 * 属性：大 payload 的响应也应该被正确序列化和反序列化。
 * 
 * 测试策略：
 * 1. 生成较大的 payload（但不超过合理限制）
 * 2. 验证大 payload 的序列化-反序列化往返
 */
RC_GTEST_PROP(RpcServerPropertyTest, LargePayloadSerialization,
              (const std::string& service_name, uint32_t request_id)) {
    RC_PRE(!service_name.empty());
    
    // 生成大 payload（1KB - 100KB）
    auto payload_size = *rc::gen::inRange(1024u, 102400u);
    std::vector<uint8_t> large_payload(payload_size);
    
    // 填充随机数据
    for (size_t i = 0; i < payload_size; ++i) {
        large_payload[i] = static_cast<uint8_t>(i % 256);
    }
    
    // 创建 RPC 服务器和序列化器
    RpcServer server;
    BinarySerializer serializer;
    
    // 注册服务处理函数
    server.register_service(service_name, 
        [large_payload](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = large_payload;
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {};
    request.timeout = std::chrono::milliseconds(5000);
    
    // 序列化请求
    auto request_bytes = serializer.serialize(request);
    
    // 处理请求，获取序列化的响应
    auto response_bytes = server.handle_raw_request(request_bytes);
    
    // 反序列化响应
    Response deserialized_response = serializer.deserialize_response(response_bytes);
    
    // 验证：request_id 匹配
    RC_ASSERT(deserialized_response.request_id == request_id);
    
    // 验证：错误码为成功
    RC_ASSERT(deserialized_response.error_code == ErrorCode::Success);
    
    // 验证：payload 大小匹配
    RC_ASSERT(deserialized_response.payload.size() == payload_size);
    
    // 验证：payload 内容完整
    RC_ASSERT(deserialized_response.payload == large_payload);
}

/**
 * Feature: frpc-framework, Property 6: RPC 服务端响应序列化 - 多个 Metadata
 * 
 * **Validates: Requirements 4.4**
 * 
 * 属性：包含多个 metadata 条目的响应应该被正确序列化和反序列化。
 * 
 * 测试策略：
 * 1. 生成包含多个 metadata 条目的响应
 * 2. 验证所有 metadata 条目都被正确保留
 */
RC_GTEST_PROP(RpcServerPropertyTest, MultipleMetadataSerialization,
              (const std::string& service_name, uint32_t request_id)) {
    RC_PRE(!service_name.empty());
    
    // 生成多个 metadata 条目
    auto num_metadata = *rc::gen::inRange(1, 11);
    std::unordered_map<std::string, std::string> metadata;
    
    for (int i = 0; i < num_metadata; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        metadata[key] = value;
    }
    
    // 创建 RPC 服务器和序列化器
    RpcServer server;
    BinarySerializer serializer;
    
    // 注册服务处理函数
    server.register_service(service_name, 
        [metadata](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = {1, 2, 3};
        resp.metadata = metadata;
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {};
    request.timeout = std::chrono::milliseconds(5000);
    
    // 序列化请求
    auto request_bytes = serializer.serialize(request);
    
    // 处理请求，获取序列化的响应
    auto response_bytes = server.handle_raw_request(request_bytes);
    
    // 反序列化响应
    Response deserialized_response = serializer.deserialize_response(response_bytes);
    
    // 验证：request_id 匹配
    RC_ASSERT(deserialized_response.request_id == request_id);
    
    // 验证：错误码为成功
    RC_ASSERT(deserialized_response.error_code == ErrorCode::Success);
    
    // 验证：metadata 数量匹配
    RC_ASSERT(deserialized_response.metadata.size() == metadata.size());
    
    // 验证：所有 metadata 条目都被正确保留
    for (const auto& [key, value] : metadata) {
        RC_ASSERT(deserialized_response.metadata.count(key) > 0);
        RC_ASSERT(deserialized_response.metadata.at(key) == value);
    }
}

/**
 * Feature: frpc-framework, Property 6: RPC 服务端响应序列化 - 空响应
 * 
 * **Validates: Requirements 4.4**
 * 
 * 属性：空 payload 和空 metadata 的响应也应该被正确序列化和反序列化。
 * 
 * 测试策略：
 * 1. 返回最小化的响应（空 payload，空 metadata）
 * 2. 验证序列化-反序列化往返
 */
RC_GTEST_PROP(RpcServerPropertyTest, EmptyResponseSerialization,
              (const std::string& service_name, uint32_t request_id)) {
    RC_PRE(!service_name.empty());
    
    // 创建 RPC 服务器和序列化器
    RpcServer server;
    BinarySerializer serializer;
    
    // 注册服务处理函数，返回空响应
    server.register_service(service_name, 
        [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = {};  // 空 payload
        resp.metadata = {};  // 空 metadata
        return resp;
    });
    
    // 创建请求
    Request request;
    request.request_id = request_id;
    request.service_name = service_name;
    request.payload = {};
    request.timeout = std::chrono::milliseconds(5000);
    
    // 序列化请求
    auto request_bytes = serializer.serialize(request);
    
    // 处理请求，获取序列化的响应
    auto response_bytes = server.handle_raw_request(request_bytes);
    
    // 反序列化响应
    Response deserialized_response = serializer.deserialize_response(response_bytes);
    
    // 验证：request_id 匹配
    RC_ASSERT(deserialized_response.request_id == request_id);
    
    // 验证：错误码为成功
    RC_ASSERT(deserialized_response.error_code == ErrorCode::Success);
    
    // 验证：错误消息为空
    RC_ASSERT(deserialized_response.error_message.empty());
    
    // 验证：payload 为空
    RC_ASSERT(deserialized_response.payload.empty());
    
    // 验证：metadata 为空
    RC_ASSERT(deserialized_response.metadata.empty());
}

// ============================================================================
// Property 7: RPC 服务端并发处理
// ============================================================================

/**
 * Feature: frpc-framework, Property 7: RPC 服务端并发处理
 * 
 * **Validates: Requirements 4.7**
 * 
 * 属性：FOR ANY 多个并发的 RPC 请求，
 * RPC Server 应该能够同时处理这些请求，
 * 且每个请求的响应应该正确匹配其对应的请求 ID。
 * 
 * 测试策略：
 * 1. 生成多个并发请求（不同的 request_id）
 * 2. 使用多线程模拟并发处理（或在 MVP 中顺序处理）
 * 3. 验证每个响应的 request_id 与请求匹配
 * 4. 验证所有请求都被正确处理
 * 5. 验证没有响应丢失或重复
 * 
 * @note 在 MVP 实现中，服务器可能顺序处理请求而非真正并发，
 *       但关键是验证响应与请求的正确匹配。
 */
RC_GTEST_PROP(RpcServerPropertyTest, ConcurrentRequestHandling,
              (const std::string& service_name)) {
    // 前置条件：服务名称非空
    RC_PRE(!service_name.empty());
    
    // 生成并发请求数量（2-20 个请求）
    auto num_requests = *rc::gen::inRange(2, 21);
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 注册服务处理函数
    // 处理函数将 request_id 编码到响应的 payload 中
    server.register_service(service_name, [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        
        // 将 request_id 编码到 payload（4 字节，大端序）
        resp.payload.resize(4);
        resp.payload[0] = (req.request_id >> 24) & 0xFF;
        resp.payload[1] = (req.request_id >> 16) & 0xFF;
        resp.payload[2] = (req.request_id >> 8) & 0xFF;
        resp.payload[3] = req.request_id & 0xFF;
        
        return resp;
    });
    
    // 生成唯一的请求 ID 列表
    std::vector<uint32_t> request_ids;
    for (int i = 0; i < num_requests; ++i) {
        request_ids.push_back(static_cast<uint32_t>(i + 1));
    }
    
    // 存储响应的容器（使用 map 以便按 request_id 查找）
    std::unordered_map<uint32_t, Response> responses;
    std::mutex responses_mutex;
    
    // 创建多个线程模拟并发请求
    std::vector<std::thread> threads;
    
    for (uint32_t request_id : request_ids) {
        threads.emplace_back([&server, &service_name, request_id, &responses, &responses_mutex]() {
            // 创建请求
            Request request;
            request.request_id = request_id;
            request.service_name = service_name;
            request.payload = {static_cast<uint8_t>(request_id & 0xFF)};
            request.timeout = std::chrono::milliseconds(5000);
            
            // 处理请求
            Response response = server.handle_request(request);
            
            // 存储响应
            std::lock_guard<std::mutex> lock(responses_mutex);
            responses[request_id] = response;
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证：收到的响应数量与请求数量相同
    RC_ASSERT(responses.size() == static_cast<size_t>(num_requests));
    
    // 验证：每个请求都有对应的响应
    for (uint32_t request_id : request_ids) {
        RC_ASSERT(responses.count(request_id) > 0);
        
        const Response& response = responses[request_id];
        
        // 验证：响应的 request_id 匹配
        RC_ASSERT(response.request_id == request_id);
        
        // 验证：请求成功
        RC_ASSERT(response.error_code == ErrorCode::Success);
        
        // 验证：payload 包含正确的 request_id
        RC_ASSERT(response.payload.size() == 4u);
        uint32_t payload_id = (static_cast<uint32_t>(response.payload[0]) << 24) |
                              (static_cast<uint32_t>(response.payload[1]) << 16) |
                              (static_cast<uint32_t>(response.payload[2]) << 8) |
                              static_cast<uint32_t>(response.payload[3]);
        RC_ASSERT(payload_id == request_id);
    }
}

/**
 * Feature: frpc-framework, Property 7: RPC 服务端并发处理 - 多服务并发
 * 
 * **Validates: Requirements 4.7**
 * 
 * 属性：并发请求可以调用不同的服务，每个响应应该正确匹配其请求。
 * 
 * 测试策略：
 * 1. 注册多个不同的服务
 * 2. 生成调用不同服务的并发请求
 * 3. 验证每个响应正确匹配其请求和服务
 */
RC_GTEST_PROP(RpcServerPropertyTest, ConcurrentMultiServiceHandling,
              (const std::vector<std::string>& service_names)) {
    // 前置条件：至少有 2 个服务，最多 10 个
    RC_PRE(service_names.size() >= 2u && service_names.size() <= 10u);
    
    // 去重服务名称
    std::vector<std::string> unique_services;
    for (const auto& name : service_names) {
        if (!name.empty() && 
            std::find(unique_services.begin(), unique_services.end(), name) == unique_services.end()) {
            unique_services.push_back(name);
        }
    }
    
    RC_PRE(unique_services.size() >= 2u);
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 为每个服务分配唯一的标识符
    std::unordered_map<std::string, uint8_t> service_ids;
    uint8_t id = 1;
    
    // 注册所有服务
    for (const auto& service_name : unique_services) {
        service_ids[service_name] = id;
        
        server.register_service(service_name, [id](const Request& req) {
            Response resp;
            resp.request_id = req.request_id;
            resp.error_code = ErrorCode::Success;
            resp.error_message = "";
            
            // 将 service_id 和 request_id 编码到 payload
            resp.payload.resize(5);
            resp.payload[0] = id;  // service_id
            resp.payload[1] = (req.request_id >> 24) & 0xFF;
            resp.payload[2] = (req.request_id >> 16) & 0xFF;
            resp.payload[3] = (req.request_id >> 8) & 0xFF;
            resp.payload[4] = req.request_id & 0xFF;
            
            return resp;
        });
        
        id++;
    }
    
    // 生成并发请求数量
    auto num_requests = *rc::gen::inRange(5, 21);
    
    // 生成请求列表（随机选择服务）
    struct RequestInfo {
        uint32_t request_id;
        std::string service_name;
        uint8_t expected_service_id;
    };
    
    std::vector<RequestInfo> request_infos;
    for (int i = 0; i < num_requests; ++i) {
        // 随机选择一个服务
        size_t service_index = i % unique_services.size();
        const std::string& service_name = unique_services[service_index];
        
        request_infos.push_back({
            static_cast<uint32_t>(i + 1),
            service_name,
            service_ids[service_name]
        });
    }
    
    // 存储响应
    std::unordered_map<uint32_t, Response> responses;
    std::mutex responses_mutex;
    
    // 创建多个线程模拟并发请求
    std::vector<std::thread> threads;
    
    for (const auto& req_info : request_infos) {
        threads.emplace_back([&server, req_info, &responses, &responses_mutex]() {
            // 创建请求
            Request request;
            request.request_id = req_info.request_id;
            request.service_name = req_info.service_name;
            request.payload = {};
            request.timeout = std::chrono::milliseconds(5000);
            
            // 处理请求
            Response response = server.handle_request(request);
            
            // 存储响应
            std::lock_guard<std::mutex> lock(responses_mutex);
            responses[req_info.request_id] = response;
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证：收到的响应数量与请求数量相同
    RC_ASSERT(responses.size() == static_cast<size_t>(num_requests));
    
    // 验证：每个请求都有正确的响应
    for (const auto& req_info : request_infos) {
        RC_ASSERT(responses.count(req_info.request_id) > 0);
        
        const Response& response = responses[req_info.request_id];
        
        // 验证：响应的 request_id 匹配
        RC_ASSERT(response.request_id == req_info.request_id);
        
        // 验证：请求成功
        RC_ASSERT(response.error_code == ErrorCode::Success);
        
        // 验证：payload 包含正确的 service_id 和 request_id
        RC_ASSERT(response.payload.size() == 5u);
        RC_ASSERT(response.payload[0] == req_info.expected_service_id);
        
        uint32_t payload_request_id = (static_cast<uint32_t>(response.payload[1]) << 24) |
                                      (static_cast<uint32_t>(response.payload[2]) << 16) |
                                      (static_cast<uint32_t>(response.payload[3]) << 8) |
                                      static_cast<uint32_t>(response.payload[4]);
        RC_ASSERT(payload_request_id == req_info.request_id);
    }
}

/**
 * Feature: frpc-framework, Property 7: RPC 服务端并发处理 - 序列化并发
 * 
 * **Validates: Requirements 4.7**
 * 
 * 属性：并发处理序列化的请求，响应也应该正确序列化并匹配。
 * 
 * 测试策略：
 * 1. 生成多个并发请求
 * 2. 序列化请求
 * 3. 并发调用 handle_raw_request
 * 4. 反序列化响应并验证
 */
RC_GTEST_PROP(RpcServerPropertyTest, ConcurrentSerializedRequestHandling,
              (const std::string& service_name)) {
    RC_PRE(!service_name.empty());
    
    // 生成并发请求数量
    auto num_requests = *rc::gen::inRange(2, 21);
    
    // 创建 RPC 服务器和序列化器
    RpcServer server;
    BinarySerializer serializer;
    
    // 注册服务
    server.register_service(service_name, [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = req.payload;  // 回显 payload
        return resp;
    });
    
    // 生成请求列表
    std::vector<Request> requests;
    for (int i = 0; i < num_requests; ++i) {
        Request request;
        request.request_id = static_cast<uint32_t>(i + 1);
        request.service_name = service_name;
        // 每个请求有唯一的 payload
        request.payload = {static_cast<uint8_t>(i & 0xFF), 
                          static_cast<uint8_t>((i >> 8) & 0xFF)};
        request.timeout = std::chrono::milliseconds(5000);
        requests.push_back(request);
    }
    
    // 存储响应
    std::unordered_map<uint32_t, Response> responses;
    std::mutex responses_mutex;
    
    // 创建多个线程模拟并发请求
    std::vector<std::thread> threads;
    
    for (const auto& request : requests) {
        threads.emplace_back([&server, &serializer, request, &responses, &responses_mutex]() {
            // 序列化请求
            auto request_bytes = serializer.serialize(request);
            
            // 处理序列化的请求
            auto response_bytes = server.handle_raw_request(request_bytes);
            
            // 反序列化响应
            Response response = serializer.deserialize_response(response_bytes);
            
            // 存储响应
            std::lock_guard<std::mutex> lock(responses_mutex);
            responses[request.request_id] = response;
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证：收到的响应数量与请求数量相同
    RC_ASSERT(responses.size() == static_cast<size_t>(num_requests));
    
    // 验证：每个请求都有正确的响应
    for (const auto& request : requests) {
        RC_ASSERT(responses.count(request.request_id) > 0);
        
        const Response& response = responses[request.request_id];
        
        // 验证：响应的 request_id 匹配
        RC_ASSERT(response.request_id == request.request_id);
        
        // 验证：请求成功
        RC_ASSERT(response.error_code == ErrorCode::Success);
        
        // 验证：payload 匹配
        RC_ASSERT(response.payload == request.payload);
    }
}

/**
 * Feature: frpc-framework, Property 7: RPC 服务端并发处理 - 无响应丢失
 * 
 * **Validates: Requirements 4.7**
 * 
 * 属性：在并发处理中，不应该有响应丢失或重复。
 * 
 * 测试策略：
 * 1. 生成大量并发请求
 * 2. 验证响应数量与请求数量完全相同
 * 3. 验证每个 request_id 只出现一次
 */
RC_GTEST_PROP(RpcServerPropertyTest, ConcurrentNoResponseLoss,
              (const std::string& service_name)) {
    RC_PRE(!service_name.empty());
    
    // 生成较多的并发请求（10-50 个）
    auto num_requests = *rc::gen::inRange(10, 51);
    
    // 创建 RPC 服务器
    RpcServer server;
    
    // 注册服务
    server.register_service(service_name, [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = {};
        return resp;
    });
    
    // 生成唯一的请求 ID 列表
    std::set<uint32_t> expected_request_ids;
    for (int i = 0; i < num_requests; ++i) {
        expected_request_ids.insert(static_cast<uint32_t>(i + 1));
    }
    
    // 存储响应的 request_id
    std::vector<uint32_t> received_request_ids;
    std::mutex ids_mutex;
    
    // 创建多个线程模拟并发请求
    std::vector<std::thread> threads;
    
    for (uint32_t request_id : expected_request_ids) {
        threads.emplace_back([&server, &service_name, request_id, &received_request_ids, &ids_mutex]() {
            // 创建请求
            Request request;
            request.request_id = request_id;
            request.service_name = service_name;
            request.payload = {};
            request.timeout = std::chrono::milliseconds(5000);
            
            // 处理请求
            Response response = server.handle_request(request);
            
            // 记录响应的 request_id
            std::lock_guard<std::mutex> lock(ids_mutex);
            received_request_ids.push_back(response.request_id);
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证：收到的响应数量与请求数量相同
    RC_ASSERT(received_request_ids.size() == static_cast<size_t>(num_requests));
    
    // 将收到的 request_id 转换为 set（自动去重）
    std::set<uint32_t> received_set(received_request_ids.begin(), received_request_ids.end());
    
    // 验证：没有重复的响应（set 大小应该等于 vector 大小）
    RC_ASSERT(received_set.size() == received_request_ids.size());
    
    // 验证：收到的 request_id 集合与期望的完全相同
    RC_ASSERT(received_set == expected_request_ids);
}

// ============================================================================
// Main Function
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
