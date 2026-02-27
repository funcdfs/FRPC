/**
 * @file serializer_example.cpp
 * @brief 二进制序列化器使用示例
 * 
 * 演示如何使用 BinarySerializer 序列化和反序列化 Request 和 Response 对象。
 */

#include <iostream>
#include <iomanip>
#include "frpc/serializer.h"
#include "frpc/exceptions.h"

using namespace frpc;

/**
 * @brief 打印字节数组（十六进制格式）
 */
void print_bytes(const ByteBuffer& bytes, size_t max_bytes = 64) {
    std::cout << "Bytes (" << bytes.size() << " total): ";
    size_t count = std::min(bytes.size(), max_bytes);
    for (size_t i = 0; i < count; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(bytes[i]) << " ";
    }
    if (bytes.size() > max_bytes) {
        std::cout << "...";
    }
    std::cout << std::dec << std::endl;
}

/**
 * @brief 示例 1: 序列化和反序列化请求
 */
void example_request_serialization() {
    std::cout << "\n=== Example 1: Request Serialization ===" << std::endl;
    
    BinarySerializer serializer;
    
    // 创建请求对象
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "user_service";
    request.payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    request.timeout = Duration(3000);
    request.metadata["trace_id"] = "abc-123-xyz";
    request.metadata["client_version"] = "1.0.0";
    
    std::cout << "Original Request:" << std::endl;
    std::cout << "  Request ID: " << request.request_id << std::endl;
    std::cout << "  Service Name: " << request.service_name << std::endl;
    std::cout << "  Payload Size: " << request.payload.size() << " bytes" << std::endl;
    std::cout << "  Timeout: " << request.timeout.count() << " ms" << std::endl;
    std::cout << "  Metadata: " << request.metadata.size() << " entries" << std::endl;
    
    // 序列化
    auto bytes = serializer.serialize(request);
    std::cout << "\nSerialized: " << std::endl;
    print_bytes(bytes);
    
    // 反序列化
    auto deserialized = serializer.deserialize_request(bytes);
    
    std::cout << "\nDeserialized Request:" << std::endl;
    std::cout << "  Request ID: " << deserialized.request_id << std::endl;
    std::cout << "  Service Name: " << deserialized.service_name << std::endl;
    std::cout << "  Payload Size: " << deserialized.payload.size() << " bytes" << std::endl;
    std::cout << "  Timeout: " << deserialized.timeout.count() << " ms" << std::endl;
    std::cout << "  Metadata: " << deserialized.metadata.size() << " entries" << std::endl;
    
    // 验证
    bool match = (deserialized.request_id == request.request_id &&
                  deserialized.service_name == request.service_name &&
                  deserialized.payload == request.payload &&
                  deserialized.timeout.count() == request.timeout.count() &&
                  deserialized.metadata == request.metadata);
    
    std::cout << "\n✓ Round-trip verification: " << (match ? "PASSED" : "FAILED") << std::endl;
}

/**
 * @brief 示例 2: 序列化和反序列化成功响应
 */
void example_success_response() {
    std::cout << "\n=== Example 2: Success Response Serialization ===" << std::endl;
    
    BinarySerializer serializer;
    
    // 创建成功响应对象
    Response response;
    response.request_id = 12345;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    response.metadata["server_version"] = "1.0.0";
    response.metadata["process_time_ms"] = "42";
    
    std::cout << "Original Response:" << std::endl;
    std::cout << "  Request ID: " << response.request_id << std::endl;
    std::cout << "  Error Code: " << static_cast<int>(response.error_code) << " (Success)" << std::endl;
    std::cout << "  Payload Size: " << response.payload.size() << " bytes" << std::endl;
    std::cout << "  Metadata: " << response.metadata.size() << " entries" << std::endl;
    
    // 序列化
    auto bytes = serializer.serialize(response);
    std::cout << "\nSerialized: " << std::endl;
    print_bytes(bytes);
    
    // 反序列化
    auto deserialized = serializer.deserialize_response(bytes);
    
    std::cout << "\nDeserialized Response:" << std::endl;
    std::cout << "  Request ID: " << deserialized.request_id << std::endl;
    std::cout << "  Error Code: " << static_cast<int>(deserialized.error_code) << std::endl;
    std::cout << "  Payload Size: " << deserialized.payload.size() << " bytes" << std::endl;
    std::cout << "  Metadata: " << deserialized.metadata.size() << " entries" << std::endl;
    
    // 验证
    bool match = (deserialized.request_id == response.request_id &&
                  deserialized.error_code == response.error_code &&
                  deserialized.payload == response.payload &&
                  deserialized.metadata == response.metadata);
    
    std::cout << "\n✓ Round-trip verification: " << (match ? "PASSED" : "FAILED") << std::endl;
}

/**
 * @brief 示例 3: 序列化和反序列化错误响应
 */
void example_error_response() {
    std::cout << "\n=== Example 3: Error Response Serialization ===" << std::endl;
    
    BinarySerializer serializer;
    
    // 创建错误响应对象
    Response response;
    response.request_id = 99999;
    response.error_code = ErrorCode::ServiceNotFound;
    response.error_message = "Service 'unknown_service' not found";
    response.payload = {};  // 错误响应通常没有 payload
    response.metadata["server_id"] = "server-01";
    
    std::cout << "Original Error Response:" << std::endl;
    std::cout << "  Request ID: " << response.request_id << std::endl;
    std::cout << "  Error Code: " << static_cast<int>(response.error_code) << std::endl;
    std::cout << "  Error Message: " << response.error_message << std::endl;
    std::cout << "  Payload Size: " << response.payload.size() << " bytes" << std::endl;
    
    // 序列化
    auto bytes = serializer.serialize(response);
    std::cout << "\nSerialized: " << std::endl;
    print_bytes(bytes);
    
    // 反序列化
    auto deserialized = serializer.deserialize_response(bytes);
    
    std::cout << "\nDeserialized Error Response:" << std::endl;
    std::cout << "  Request ID: " << deserialized.request_id << std::endl;
    std::cout << "  Error Code: " << static_cast<int>(deserialized.error_code) << std::endl;
    std::cout << "  Error Message: " << deserialized.error_message << std::endl;
    std::cout << "  Payload Size: " << deserialized.payload.size() << " bytes" << std::endl;
    
    // 验证
    bool match = (deserialized.request_id == response.request_id &&
                  deserialized.error_code == response.error_code &&
                  deserialized.error_message == response.error_message);
    
    std::cout << "\n✓ Round-trip verification: " << (match ? "PASSED" : "FAILED") << std::endl;
}

/**
 * @brief 示例 4: 错误处理 - 无效格式
 */
void example_error_handling() {
    std::cout << "\n=== Example 4: Error Handling ===" << std::endl;
    
    BinarySerializer serializer;
    
    // 测试 1: 无效的魔数
    std::cout << "\nTest 1: Invalid magic number" << std::endl;
    ByteBuffer invalid_magic = {
        0xFF, 0xFF, 0xFF, 0xFF,  // 错误的魔数
        0x00, 0x00, 0x00, 0x01,  // 版本
        0x00, 0x00, 0x00, 0x00,  // 消息类型
    };
    
    try {
        serializer.deserialize_request(invalid_magic);
        std::cout << "  ✗ Should have thrown InvalidFormatException" << std::endl;
    } catch (const InvalidFormatException& e) {
        std::cout << "  ✓ Caught InvalidFormatException: " << e.what() << std::endl;
    }
    
    // 测试 2: 数据不完整
    std::cout << "\nTest 2: Insufficient data" << std::endl;
    ByteBuffer incomplete_data = {
        0x46, 0x52, 0x50, 0x43,  // 魔数 "FRPC"
        0x00, 0x00, 0x00, 0x01,  // 版本
        // 缺少消息类型和其他字段
    };
    
    try {
        serializer.deserialize_request(incomplete_data);
        std::cout << "  ✗ Should have thrown DeserializationException" << std::endl;
    } catch (const DeserializationException& e) {
        std::cout << "  ✓ Caught DeserializationException: " << e.what() << std::endl;
    }
    
    // 测试 3: 消息类型不匹配
    std::cout << "\nTest 3: Wrong message type" << std::endl;
    Response response;
    response.request_id = 1;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload = {};
    
    auto response_bytes = serializer.serialize(response);
    
    try {
        // 尝试将响应数据反序列化为请求
        serializer.deserialize_request(response_bytes);
        std::cout << "  ✗ Should have thrown InvalidFormatException" << std::endl;
    } catch (const InvalidFormatException& e) {
        std::cout << "  ✓ Caught InvalidFormatException: " << e.what() << std::endl;
    }
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << "FRPC Binary Serializer Examples" << std::endl;
    std::cout << "================================" << std::endl;
    
    try {
        example_request_serialization();
        example_success_response();
        example_error_response();
        example_error_handling();
        
        std::cout << "\n=== All Examples Completed Successfully ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << std::endl;
        return 1;
    }
}
