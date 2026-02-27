/**
 * @file test_serializer.cpp
 * @brief 二进制序列化器单元测试
 * 
 * 测试 BinarySerializer 的序列化和反序列化功能。
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "frpc/serializer.h"
#include "frpc/exceptions.h"

using namespace frpc;

// ============================================================================
// RapidCheck 生成器
// ============================================================================

namespace rc {

/**
 * @brief Request 对象的自定义生成器
 * 
 * 生成随机的 Request 对象用于属性测试。
 */
template<>
struct Arbitrary<Request> {
    static Gen<Request> arbitrary() {
        return gen::build<Request>(
            gen::set(&Request::request_id, gen::inRange<uint32_t>(1, 1000000)),
            gen::set(&Request::service_name, gen::string<std::string>()),
            gen::set(&Request::payload, gen::container<ByteBuffer>(gen::arbitrary<Byte>())),
            gen::set(&Request::timeout, gen::map(
                gen::inRange(100, 10000),
                [](int ms) { return Duration(ms); }
            )),
            gen::set(&Request::metadata, gen::map(
                gen::container<std::vector<std::pair<std::string, std::string>>>(
                    gen::pair(gen::string<std::string>(), gen::string<std::string>())
                ),
                [](const std::vector<std::pair<std::string, std::string>>& pairs) {
                    std::unordered_map<std::string, std::string> map;
                    for (const auto& [k, v] : pairs) {
                        map[k] = v;
                    }
                    return map;
                }
            ))
        );
    }
};

/**
 * @brief Response 对象的自定义生成器
 * 
 * 生成随机的 Response 对象用于属性测试。
 */
template<>
struct Arbitrary<Response> {
    static Gen<Response> arbitrary() {
        return gen::build<Response>(
            gen::set(&Response::request_id, gen::inRange<uint32_t>(1, 1000000)),
            gen::set(&Response::error_code, gen::element(
                ErrorCode::Success,
                ErrorCode::NetworkError,
                ErrorCode::SerializationError,
                ErrorCode::ServiceNotFound,
                ErrorCode::Timeout
            )),
            gen::set(&Response::error_message, gen::string<std::string>()),
            gen::set(&Response::payload, gen::container<ByteBuffer>(gen::arbitrary<Byte>())),
            gen::set(&Response::metadata, gen::map(
                gen::container<std::vector<std::pair<std::string, std::string>>>(
                    gen::pair(gen::string<std::string>(), gen::string<std::string>())
                ),
                [](const std::vector<std::pair<std::string, std::string>>& pairs) {
                    std::unordered_map<std::string, std::string> map;
                    for (const auto& [k, v] : pairs) {
                        map[k] = v;
                    }
                    return map;
                }
            ))
        );
    }
};

} // namespace rc

// ============================================================================
// 属性测试 (Property-Based Tests)
// ============================================================================

/**
 * Feature: frpc-framework, Property 10: Request 序列化往返
 * 
 * **Validates: Requirements 6.5**
 * 
 * 测试任意有效 Request 对象序列化后反序列化产生等价对象。
 * 
 * 对于所有有效的 Request 对象，序列化后反序列化应该产生等价的对象
 * （所有字段值相同）。这是序列化器正确性的核心属性。
 */
RC_GTEST_PROP(SerializerPropertyTest, RequestRoundTrip, ()) {
    // 生成随机的 Request 对象
    auto request = *rc::gen::arbitrary<Request>();
    
    BinarySerializer serializer;
    
    // 序列化
    auto bytes = serializer.serialize(request);
    
    // 反序列化
    auto deserialized = serializer.deserialize_request(bytes);
    
    // 验证等价性：所有字段都应该相同
    RC_ASSERT(deserialized.request_id == request.request_id);
    RC_ASSERT(deserialized.service_name == request.service_name);
    RC_ASSERT(deserialized.payload == request.payload);
    RC_ASSERT(deserialized.timeout.count() == request.timeout.count());
    RC_ASSERT(deserialized.metadata == request.metadata);
}

/**
 * Feature: frpc-framework, Property 11: Response 序列化往返
 * 
 * **Validates: Requirements 6.6**
 * 
 * 测试任意有效 Response 对象序列化后反序列化产生等价对象。
 * 
 * 对于所有有效的 Response 对象，序列化后反序列化应该产生等价的对象
 * （所有字段值相同）。这是序列化器正确性的核心属性。
 */
RC_GTEST_PROP(SerializerPropertyTest, ResponseRoundTrip, ()) {
    // 生成随机的 Response 对象
    auto response = *rc::gen::arbitrary<Response>();
    
    BinarySerializer serializer;
    
    // 序列化
    auto bytes = serializer.serialize(response);
    
    // 反序列化
    auto deserialized = serializer.deserialize_response(bytes);
    
    // 验证等价性：所有字段都应该相同
    RC_ASSERT(deserialized.request_id == response.request_id);
    RC_ASSERT(deserialized.error_code == response.error_code);
    RC_ASSERT(deserialized.error_message == response.error_message);
    RC_ASSERT(deserialized.payload == response.payload);
    RC_ASSERT(deserialized.metadata == response.metadata);
}

// ============================================================================
// 基础功能测试
// ============================================================================

TEST(BinarySerializerTest, SerializeDeserializeRequest) {
    BinarySerializer serializer;
    
    // 创建请求对象
    Request request;
    request.request_id = 12345;
    request.service_name = "test_service";
    request.payload = {1, 2, 3, 4, 5};
    request.timeout = Duration(3000);
    request.metadata["trace_id"] = "abc-123";
    request.metadata["client_version"] = "1.0.0";
    
    // 序列化
    auto bytes = serializer.serialize(request);
    ASSERT_FALSE(bytes.empty());
    
    // 反序列化
    auto deserialized = serializer.deserialize_request(bytes);
    
    // 验证等价性
    EXPECT_EQ(deserialized.request_id, request.request_id);
    EXPECT_EQ(deserialized.service_name, request.service_name);
    EXPECT_EQ(deserialized.payload, request.payload);
    EXPECT_EQ(deserialized.timeout.count(), request.timeout.count());
    EXPECT_EQ(deserialized.metadata, request.metadata);
}

TEST(BinarySerializerTest, SerializeDeserializeResponse) {
    BinarySerializer serializer;
    
    // 创建响应对象
    Response response;
    response.request_id = 12345;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload = {10, 20, 30, 40, 50};
    response.metadata["server_version"] = "1.0.0";
    response.metadata["process_time_ms"] = "42";
    
    // 序列化
    auto bytes = serializer.serialize(response);
    ASSERT_FALSE(bytes.empty());
    
    // 反序列化
    auto deserialized = serializer.deserialize_response(bytes);
    
    // 验证等价性
    EXPECT_EQ(deserialized.request_id, response.request_id);
    EXPECT_EQ(deserialized.error_code, response.error_code);
    EXPECT_EQ(deserialized.error_message, response.error_message);
    EXPECT_EQ(deserialized.payload, response.payload);
    EXPECT_EQ(deserialized.metadata, response.metadata);
}

TEST(BinarySerializerTest, SerializeDeserializeErrorResponse) {
    BinarySerializer serializer;
    
    // 创建错误响应对象
    Response response;
    response.request_id = 99999;
    response.error_code = ErrorCode::ServiceNotFound;
    response.error_message = "Service 'unknown_service' not found";
    response.payload = {};  // 错误响应通常没有 payload
    
    // 序列化
    auto bytes = serializer.serialize(response);
    ASSERT_FALSE(bytes.empty());
    
    // 反序列化
    auto deserialized = serializer.deserialize_response(bytes);
    
    // 验证等价性
    EXPECT_EQ(deserialized.request_id, response.request_id);
    EXPECT_EQ(deserialized.error_code, response.error_code);
    EXPECT_EQ(deserialized.error_message, response.error_message);
    EXPECT_EQ(deserialized.payload, response.payload);
}

// ============================================================================
// 边缘情况测试
// ============================================================================

TEST(BinarySerializerTest, EmptyServiceName) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 1;
    request.service_name = "";  // 空服务名
    request.payload = {};
    request.timeout = Duration(1000);
    
    auto bytes = serializer.serialize(request);
    auto deserialized = serializer.deserialize_request(bytes);
    
    EXPECT_EQ(deserialized.service_name, "");
}

TEST(BinarySerializerTest, EmptyPayload) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 1;
    request.service_name = "test";
    request.payload = {};  // 空 payload
    request.timeout = Duration(1000);
    
    auto bytes = serializer.serialize(request);
    auto deserialized = serializer.deserialize_request(bytes);
    
    EXPECT_TRUE(deserialized.payload.empty());
}

TEST(BinarySerializerTest, EmptyMetadata) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 1;
    request.service_name = "test";
    request.payload = {1, 2, 3};
    request.timeout = Duration(1000);
    request.metadata = {};  // 空元数据
    
    auto bytes = serializer.serialize(request);
    auto deserialized = serializer.deserialize_request(bytes);
    
    EXPECT_TRUE(deserialized.metadata.empty());
}

TEST(BinarySerializerTest, LargePayload) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 1;
    request.service_name = "test";
    // 创建 1MB 的 payload
    request.payload.resize(1024 * 1024, 0xAB);
    request.timeout = Duration(5000);
    
    auto bytes = serializer.serialize(request);
    auto deserialized = serializer.deserialize_request(bytes);
    
    EXPECT_EQ(deserialized.payload.size(), request.payload.size());
    EXPECT_EQ(deserialized.payload, request.payload);
}

TEST(BinarySerializerTest, LongServiceName) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 1;
    // 创建很长的服务名
    request.service_name = std::string(1000, 'x');
    request.payload = {1, 2, 3};
    request.timeout = Duration(1000);
    
    auto bytes = serializer.serialize(request);
    auto deserialized = serializer.deserialize_request(bytes);
    
    EXPECT_EQ(deserialized.service_name, request.service_name);
}

TEST(BinarySerializerTest, ManyMetadataEntries) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 1;
    request.service_name = "test";
    request.payload = {1, 2, 3};
    request.timeout = Duration(1000);
    
    // 添加多个元数据条目
    for (int i = 0; i < 100; ++i) {
        request.metadata["key_" + std::to_string(i)] = "value_" + std::to_string(i);
    }
    
    auto bytes = serializer.serialize(request);
    auto deserialized = serializer.deserialize_request(bytes);
    
    EXPECT_EQ(deserialized.metadata.size(), 100);
    EXPECT_EQ(deserialized.metadata, request.metadata);
}

// ============================================================================
// 错误处理测试
// ============================================================================

TEST(BinarySerializerTest, InvalidMagicNumber) {
    BinarySerializer serializer;
    
    // 创建无效的数据（错误的魔数）
    ByteBuffer invalid_data = {
        0xFF, 0xFF, 0xFF, 0xFF,  // 错误的魔数
        0x00, 0x00, 0x00, 0x01,  // 版本
        0x00, 0x00, 0x00, 0x00,  // 消息类型
    };
    
    EXPECT_THROW(serializer.deserialize_request(invalid_data), InvalidFormatException);
}

TEST(BinarySerializerTest, InvalidVersion) {
    BinarySerializer serializer;
    
    // 创建无效的数据（错误的版本）
    ByteBuffer invalid_data = {
        0x46, 0x52, 0x50, 0x43,  // 正确的魔数 "FRPC"
        0x00, 0x00, 0x00, 0xFF,  // 错误的版本
        0x00, 0x00, 0x00, 0x00,  // 消息类型
    };
    
    EXPECT_THROW(serializer.deserialize_request(invalid_data), InvalidFormatException);
}

TEST(BinarySerializerTest, WrongMessageType) {
    BinarySerializer serializer;
    
    // 创建响应数据
    Response response;
    response.request_id = 1;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload = {};
    
    auto bytes = serializer.serialize(response);
    
    // 尝试将响应数据反序列化为请求
    EXPECT_THROW(serializer.deserialize_request(bytes), InvalidFormatException);
}

TEST(BinarySerializerTest, InsufficientData) {
    BinarySerializer serializer;
    
    // 创建不完整的数据（只有部分头部）
    ByteBuffer incomplete_data = {
        0x46, 0x52, 0x50, 0x43,  // 魔数
        0x00, 0x00, 0x00, 0x01,  // 版本
        // 缺少消息类型和其他字段
    };
    
    EXPECT_THROW(serializer.deserialize_request(incomplete_data), DeserializationException);
}

TEST(BinarySerializerTest, TruncatedString) {
    BinarySerializer serializer;
    
    // 创建包含截断字符串的数据
    ByteBuffer truncated_data = {
        0x46, 0x52, 0x50, 0x43,  // 魔数
        0x00, 0x00, 0x00, 0x01,  // 版本
        0x00, 0x00, 0x00, 0x00,  // 消息类型 (Request)
        0x00, 0x00, 0x00, 0x01,  // 请求 ID
        0x00, 0x00, 0x00, 0x0A,  // 服务名长度 = 10
        0x74, 0x65, 0x73, 0x74,  // "test" (只有 4 字节，但声称有 10 字节)
    };
    
    EXPECT_THROW(serializer.deserialize_request(truncated_data), DeserializationException);
}

// ============================================================================
// 协议格式验证测试
// ============================================================================

TEST(BinarySerializerTest, VerifyRequestFormat) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 0x12345678;
    request.service_name = "test";
    request.payload = {0xAA, 0xBB};
    request.timeout = Duration(1000);
    
    auto bytes = serializer.serialize(request);
    
    // 验证魔数
    EXPECT_EQ(bytes[0], 0x46);  // 'F'
    EXPECT_EQ(bytes[1], 0x52);  // 'R'
    EXPECT_EQ(bytes[2], 0x50);  // 'P'
    EXPECT_EQ(bytes[3], 0x43);  // 'C'
    
    // 验证版本（网络字节序）
    EXPECT_EQ(bytes[4], 0x00);
    EXPECT_EQ(bytes[5], 0x00);
    EXPECT_EQ(bytes[6], 0x00);
    EXPECT_EQ(bytes[7], 0x01);
    
    // 验证消息类型（0 = Request）
    EXPECT_EQ(bytes[8], 0x00);
    EXPECT_EQ(bytes[9], 0x00);
    EXPECT_EQ(bytes[10], 0x00);
    EXPECT_EQ(bytes[11], 0x00);
}

TEST(BinarySerializerTest, VerifyResponseFormat) {
    BinarySerializer serializer;
    
    Response response;
    response.request_id = 0x12345678;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload = {0xCC, 0xDD};
    
    auto bytes = serializer.serialize(response);
    
    // 验证魔数
    EXPECT_EQ(bytes[0], 0x46);  // 'F'
    EXPECT_EQ(bytes[1], 0x52);  // 'R'
    EXPECT_EQ(bytes[2], 0x50);  // 'P'
    EXPECT_EQ(bytes[3], 0x43);  // 'C'
    
    // 验证版本（网络字节序）
    EXPECT_EQ(bytes[4], 0x00);
    EXPECT_EQ(bytes[5], 0x00);
    EXPECT_EQ(bytes[6], 0x00);
    EXPECT_EQ(bytes[7], 0x01);
    
    // 验证消息类型（1 = Response）
    EXPECT_EQ(bytes[8], 0x00);
    EXPECT_EQ(bytes[9], 0x00);
    EXPECT_EQ(bytes[10], 0x00);
    EXPECT_EQ(bytes[11], 0x01);
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
