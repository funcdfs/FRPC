/**
 * @file data_models_example.cpp
 * @brief 数据模型使用示例
 * 
 * 演示如何创建和使用 Request 和 Response 对象。
 */

#include "frpc/data_models.h"
#include <iostream>
#include <iomanip>

using namespace frpc;

/**
 * @brief 打印 Request 对象的信息
 */
void print_request(const Request& request) {
    std::cout << "Request Information:" << std::endl;
    std::cout << "  Request ID: " << request.request_id << std::endl;
    std::cout << "  Service Name: " << request.service_name << std::endl;
    std::cout << "  Payload Size: " << request.payload.size() << " bytes" << std::endl;
    std::cout << "  Timeout: " << request.timeout.count() << " ms" << std::endl;
    
    if (!request.metadata.empty()) {
        std::cout << "  Metadata:" << std::endl;
        for (const auto& [key, value] : request.metadata) {
            std::cout << "    " << key << ": " << value << std::endl;
        }
    }
    std::cout << std::endl;
}

/**
 * @brief 打印 Response 对象的信息
 */
void print_response(const Response& response) {
    std::cout << "Response Information:" << std::endl;
    std::cout << "  Request ID: " << response.request_id << std::endl;
    std::cout << "  Error Code: " << static_cast<uint32_t>(response.error_code);
    
    if (response.error_code == ErrorCode::Success) {
        std::cout << " (Success)" << std::endl;
    } else {
        std::cout << " (Error)" << std::endl;
        std::cout << "  Error Message: " << response.error_message << std::endl;
    }
    
    std::cout << "  Payload Size: " << response.payload.size() << " bytes" << std::endl;
    
    if (!response.metadata.empty()) {
        std::cout << "  Metadata:" << std::endl;
        for (const auto& [key, value] : response.metadata) {
            std::cout << "    " << key << ": " << value << std::endl;
        }
    }
    std::cout << std::endl;
}

/**
 * @brief 示例 1: 创建简单的请求
 */
void example_simple_request() {
    std::cout << "=== Example 1: Simple Request ===" << std::endl;
    
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "echo_service";
    request.payload = {72, 101, 108, 108, 111};  // "Hello" in ASCII
    request.timeout = std::chrono::milliseconds(3000);
    
    print_request(request);
}

/**
 * @brief 示例 2: 创建带元数据的请求
 */
void example_request_with_metadata() {
    std::cout << "=== Example 2: Request with Metadata ===" << std::endl;
    
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "user_service";
    request.payload = {1, 2, 3, 4};
    request.timeout = std::chrono::milliseconds(5000);
    request.metadata["trace_id"] = "abc-123-def-456";
    request.metadata["auth_token"] = "Bearer token123";
    request.metadata["client_version"] = "1.0.0";
    
    print_request(request);
}

/**
 * @brief 示例 3: 创建成功响应
 */
void example_success_response() {
    std::cout << "=== Example 3: Success Response ===" << std::endl;
    
    // 模拟请求
    uint32_t request_id = Request::generate_id();
    
    // 创建成功响应
    Response response;
    response.request_id = request_id;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload = {87, 111, 114, 108, 100};  // "World" in ASCII
    response.metadata["server_id"] = "server-1";
    response.metadata["process_time_ms"] = "42";
    
    print_response(response);
}

/**
 * @brief 示例 4: 创建错误响应
 */
void example_error_response() {
    std::cout << "=== Example 4: Error Response ===" << std::endl;
    
    // 模拟请求
    uint32_t request_id = Request::generate_id();
    
    // 创建错误响应
    Response response;
    response.request_id = request_id;
    response.error_code = ErrorCode::ServiceNotFound;
    response.error_message = "Service 'unknown_service' not found";
    response.payload = {};  // 错误响应通常没有 payload
    response.metadata["server_id"] = "server-2";
    
    print_response(response);
}

/**
 * @brief 示例 5: 生成多个唯一的请求 ID
 */
void example_generate_multiple_ids() {
    std::cout << "=== Example 5: Generate Multiple Request IDs ===" << std::endl;
    
    std::cout << "Generating 10 unique request IDs:" << std::endl;
    for (int i = 0; i < 10; ++i) {
        uint32_t id = Request::generate_id();
        std::cout << "  ID " << (i + 1) << ": " << id << std::endl;
    }
    std::cout << std::endl;
}

/**
 * @brief 示例 6: 请求-响应匹配
 */
void example_request_response_matching() {
    std::cout << "=== Example 6: Request-Response Matching ===" << std::endl;
    
    // 创建请求
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "calculator_service";
    request.payload = {10, 20};  // 模拟参数
    request.timeout = std::chrono::milliseconds(2000);
    
    std::cout << "Sending request..." << std::endl;
    print_request(request);
    
    // 创建对应的响应
    Response response;
    response.request_id = request.request_id;  // 匹配请求 ID
    response.error_code = ErrorCode::Success;
    response.payload = {30};  // 模拟结果 (10 + 20 = 30)
    
    std::cout << "Received response..." << std::endl;
    print_response(response);
    
    // 验证 ID 匹配
    if (response.request_id == request.request_id) {
        std::cout << "✓ Request and Response IDs match!" << std::endl;
    } else {
        std::cout << "✗ Request and Response IDs do NOT match!" << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "FRPC Data Models Examples" << std::endl;
    std::cout << "=========================" << std::endl;
    std::cout << std::endl;
    
    // 运行所有示例
    example_simple_request();
    example_request_with_metadata();
    example_success_response();
    example_error_response();
    example_generate_multiple_ids();
    example_request_response_matching();
    
    std::cout << "All examples completed successfully!" << std::endl;
    
    return 0;
}
