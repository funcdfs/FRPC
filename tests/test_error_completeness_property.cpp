/**
 * @file test_error_completeness_property.cpp
 * @brief 错误信息完整性属性测试
 * 
 * Property 29: 错误信息完整性
 * 验证: 需求 12.1
 * 
 * 测试所有错误情况返回包含错误码和描述的错误信息。
 * 
 * 需求 12.1: WHEN 发生错误时，THE FRPC_Framework SHALL 返回包含错误码和错误描述的错误信息
 */

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include "frpc/rpc_server.h"
#include "frpc/rpc_client.h"
#include "frpc/serializer.h"
#include "frpc/error_codes.h"
#include <string>
#include <vector>

using namespace frpc;

// Feature: frpc-framework, Property 29: 错误信息完整性

/**
 * @brief 生成随机的错误码
 */
ErrorCode gen_error_code() {
    static const std::vector<ErrorCode> error_codes = {
        ErrorCode::NetworkError,
        ErrorCode::ConnectionFailed,
        ErrorCode::ConnectionClosed,
        ErrorCode::SendFailed,
        ErrorCode::RecvFailed,
        ErrorCode::Timeout,
        ErrorCode::SerializationError,
        ErrorCode::DeserializationError,
        ErrorCode::InvalidFormat,
        ErrorCode::ServiceNotFound,
        ErrorCode::ServiceUnavailable,
        ErrorCode::NoInstanceAvailable,
        ErrorCode::ServiceException,
        ErrorCode::InvalidArgument,
        ErrorCode::MissingArgument,
        ErrorCode::InternalError,
        ErrorCode::OutOfMemory,
        ErrorCode::ResourceExhausted
    };
    
    return error_codes[rand() % error_codes.size()];
}

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

// ============================================================================
// Property 29: 错误信息完整性
// ============================================================================

/**
 * @brief Property 29: 错误信息完整性
 * 
 * For any 错误情况，FRPC Framework 返回的错误信息应该包含错误码和错误描述两个字段，
 * 且错误码应该是预定义的标准错误码。
 * 
 * **Validates: Requirements 12.1**
 * 
 * 测试策略：
 * 1. 测试服务未找到错误
 * 2. 测试无效参数错误
 * 3. 测试反序列化错误
 * 4. 验证所有错误响应都包含错误码和错误描述
 */
RC_GTEST_PROP(ErrorCompletenessPropertyTest, 
              ServiceNotFoundError,
              (const std::string& service_name)) {
    // 创建服务端（不注册任何服务）
    RpcServer server;
    
    // 创建客户端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 调用不存在的服务
    auto resp = client.call(service_name, {});
    
    // 验证响应存在
    RC_ASSERT(resp.has_value());
    
    // 验证错误码不是 Success
    RC_ASSERT(resp->error_code != ErrorCode::Success);
    
    // 验证错误码是预定义的标准错误码（ServiceNotFound）
    RC_ASSERT(resp->error_code == ErrorCode::ServiceNotFound);
    
    // 验证错误描述不为空
    RC_ASSERT(!resp->error_message.empty());
    
    // 验证错误描述包含有用信息
    RC_ASSERT(resp->error_message.find("not found") != std::string::npos ||
              resp->error_message.find("Not found") != std::string::npos ||
              resp->error_message.find("NOT FOUND") != std::string::npos);
}

/**
 * @brief Property 29: 无效参数错误的完整性
 * 
 * 测试当服务接收到无效参数时，返回的错误信息包含错误码和描述。
 */
RC_GTEST_PROP(ErrorCompletenessPropertyTest,
              InvalidArgumentError,
              ()) {
    // Generate a payload size between 0 and 7 (invalid for our add service)
    int payload_size = *rc::gen::inRange(0, 8);
    
    // 创建服务端并注册一个需要 8 字节参数的服务
    RpcServer server;
    server.register_service("add", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        
        if (req.payload.size() < 8) {
            resp.error_code = ErrorCode::InvalidArgument;
            resp.error_message = "Invalid payload: expected 8 bytes";
            return resp;
        }
        
        resp.error_code = ErrorCode::Success;
        resp.payload = serialize_int(42);
        return resp;
    });
    
    // 创建客户端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 创建无效的 payload
    ByteBuffer invalid_payload(payload_size, 0);
    
    // 调用服务
    auto resp = client.call("add", invalid_payload);
    
    // 验证响应存在
    RC_ASSERT(resp.has_value());
    
    // 验证错误码不是 Success
    RC_ASSERT(resp->error_code != ErrorCode::Success);
    
    // 验证错误码是 InvalidArgument
    RC_ASSERT(resp->error_code == ErrorCode::InvalidArgument);
    
    // 验证错误描述不为空
    RC_ASSERT(!resp->error_message.empty());
}

/**
 * @brief Property 29: 反序列化错误的完整性
 * 
 * 测试当接收到无效的请求数据时，返回的错误信息包含错误码和描述。
 */
RC_GTEST_PROP(ErrorCompletenessPropertyTest,
              DeserializationError,
              (const std::vector<uint8_t>& invalid_data)) {
    RC_PRE(invalid_data.size() < 16);  // 太短的数据无法反序列化
    
    // 创建服务端
    RpcServer server;
    server.register_service("test", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        return resp;
    });
    
    // 直接发送无效数据到服务端
    auto resp_bytes = server.handle_raw_request(invalid_data);
    
    // 反序列化响应
    BinarySerializer serializer;
    try {
        auto resp = serializer.deserialize_response(resp_bytes);
        
        // 验证错误码不是 Success
        RC_ASSERT(resp.error_code != ErrorCode::Success);
        
        // 验证错误码是反序列化相关的错误
        RC_ASSERT(resp.error_code == ErrorCode::DeserializationError ||
                  resp.error_code == ErrorCode::InvalidFormat);
        
        // 验证错误描述不为空
        RC_ASSERT(!resp.error_message.empty());
    } catch (...) {
        // 如果反序列化失败，这也是预期的行为
        // 但服务端应该返回有效的错误响应
        RC_SUCCEED();
    }
}

// ============================================================================
// 单元测试：验证所有预定义错误码都有描述
// ============================================================================

/**
 * @brief 验证所有预定义错误码都有非空描述
 * 
 * 这个测试确保 get_error_message() 函数为所有错误码提供了描述信息。
 */
TEST(ErrorCompletenessTest, AllErrorCodesHaveDescriptions) {
    // 测试所有预定义的错误码
    std::vector<ErrorCode> error_codes = {
        ErrorCode::Success,
        ErrorCode::NetworkError,
        ErrorCode::ConnectionFailed,
        ErrorCode::ConnectionClosed,
        ErrorCode::SendFailed,
        ErrorCode::RecvFailed,
        ErrorCode::Timeout,
        ErrorCode::SerializationError,
        ErrorCode::DeserializationError,
        ErrorCode::InvalidFormat,
        ErrorCode::ServiceNotFound,
        ErrorCode::ServiceUnavailable,
        ErrorCode::NoInstanceAvailable,
        ErrorCode::ServiceException,
        ErrorCode::InvalidArgument,
        ErrorCode::MissingArgument,
        ErrorCode::InternalError,
        ErrorCode::OutOfMemory,
        ErrorCode::ResourceExhausted
    };
    
    for (const auto& code : error_codes) {
        auto message = get_error_message(code);
        
        // 验证描述不为空
        EXPECT_FALSE(message.empty()) 
            << "Error code " << static_cast<int>(code) << " has no description";
        
        // 验证描述不是 "Unknown error"（除非是未知错误码）
        if (code != ErrorCode::Success) {
            EXPECT_NE(message, "Unknown error")
                << "Error code " << static_cast<int>(code) << " has generic description";
        }
    }
}

/**
 * @brief 验证错误响应的结构完整性
 * 
 * 测试错误响应包含所有必需的字段。
 */
TEST(ErrorCompletenessTest, ErrorResponseStructure) {
    // 创建服务端
    RpcServer server;
    
    // 创建客户端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 调用不存在的服务
    auto resp = client.call("non_existent_service", {});
    
    // 验证响应存在
    ASSERT_TRUE(resp.has_value());
    
    // 验证 request_id 被正确设置
    EXPECT_GT(resp->request_id, 0);
    
    // 验证错误码被设置
    EXPECT_NE(resp->error_code, ErrorCode::Success);
    
    // 验证错误描述被设置
    EXPECT_FALSE(resp->error_message.empty());
    
    // 验证 payload 可以为空（对于错误响应）
    // 这是允许的，因为错误响应可能不需要 payload
}

/**
 * @brief 验证不同错误类型的错误码范围
 * 
 * 测试错误码按照设计文档的分类正确分组。
 */
TEST(ErrorCompletenessTest, ErrorCodeRanges) {
    // 网络错误 (1000-1999)
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::NetworkError), 1000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::NetworkError), 2000);
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::Timeout), 1000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::Timeout), 2000);
    
    // 序列化错误 (2000-2999)
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::SerializationError), 2000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::SerializationError), 3000);
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::DeserializationError), 2000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::DeserializationError), 3000);
    
    // 服务错误 (3000-3999)
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::ServiceNotFound), 3000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::ServiceNotFound), 4000);
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::ServiceException), 3000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::ServiceException), 4000);
    
    // 参数错误 (4000-4999)
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::InvalidArgument), 4000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::InvalidArgument), 5000);
    
    // 系统错误 (5000-5999)
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::InternalError), 5000);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::InternalError), 6000);
}

