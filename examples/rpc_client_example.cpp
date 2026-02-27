/**
 * @file rpc_client_example.cpp
 * @brief RPC 客户端使用示例
 * 
 * 演示如何使用 FRPC 框架的 RPC 客户端调用远程服务。
 * 此示例展示了客户端-服务端的完整交互流程。
 */

#include "frpc/rpc_client.h"
#include "frpc/rpc_server.h"
#include "frpc/serializer.h"
#include <iostream>
#include <string>
#include <vector>

using namespace frpc;

/**
 * @brief 简单的整数序列化
 * 
 * 将整数转换为字节数组（大端序）
 */
ByteBuffer serialize_int(int value) {
    ByteBuffer buffer(4);
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
    return buffer;
}

/**
 * @brief 简单的整数反序列化
 * 
 * 从字节数组恢复整数（大端序）
 */
int deserialize_int(const ByteBuffer& buffer) {
    if (buffer.size() < 4) {
        return 0;
    }
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/**
 * @brief 简单的字符串序列化
 */
ByteBuffer serialize_string(const std::string& str) {
    ByteBuffer buffer(str.begin(), str.end());
    return buffer;
}

/**
 * @brief 简单的字符串反序列化
 */
std::string deserialize_string(const ByteBuffer& buffer) {
    return std::string(buffer.begin(), buffer.end());
}

/**
 * @brief 设置示例服务
 * 
 * 注册多个示例服务到服务端
 */
void setup_example_services(RpcServer& server) {
    // 1. Echo 服务：返回相同的输入
    server.register_service("echo", [](const Request& req) {
        std::cout << "[Server] Echo service called" << std::endl;
        
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.payload = req.payload;
        
        return resp;
    });

    // 2. Add 服务：两个整数相加
    server.register_service("add", [](const Request& req) {
        std::cout << "[Server] Add service called" << std::endl;
        
        Response resp;
        resp.request_id = req.request_id;
        
        // 解析参数（假设 payload 包含两个 4 字节整数）
        if (req.payload.size() >= 8) {
            ByteBuffer first_bytes(req.payload.begin(), req.payload.begin() + 4);
            ByteBuffer second_bytes(req.payload.begin() + 4, req.payload.begin() + 8);
            
            int a = deserialize_int(first_bytes);
            int b = deserialize_int(second_bytes);
            int result = a + b;
            
            std::cout << "[Server] Computing: " << a << " + " << b << " = " << result << std::endl;
            
            resp.error_code = ErrorCode::Success;
            resp.payload = serialize_int(result);
        } else {
            resp.error_code = ErrorCode::InvalidArgument;
            resp.error_message = "Invalid arguments: expected 8 bytes";
        }
        
        return resp;
    });

    // 3. Greet 服务：问候服务
    server.register_service("greet", [](const Request& req) {
        std::cout << "[Server] Greet service called" << std::endl;
        
        Response resp;
        resp.request_id = req.request_id;
        
        std::string name = deserialize_string(req.payload);
        std::string greeting = "Hello, " + name + "!";
        
        std::cout << "[Server] Greeting: " << greeting << std::endl;
        
        resp.error_code = ErrorCode::Success;
        resp.payload = serialize_string(greeting);
        
        return resp;
    });

    // 4. Error 服务：总是返回错误（用于测试错误处理）
    server.register_service("error_service", [](const Request& req) {
        std::cout << "[Server] Error service called (will return error)" << std::endl;
        
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::ServiceException;
        resp.error_message = "This service always fails for demonstration";
        
        return resp;
    });

    std::cout << "[Server] Registered " << server.service_count() << " services" << std::endl;
}

/**
 * @brief 示例 1: 基本的 RPC 调用
 */
void example_basic_call(RpcClient& client) {
    std::cout << "\n=== Example 1: Basic RPC Call ===" << std::endl;
    
    // 准备数据
    std::string message = "Hello, FRPC!";
    ByteBuffer payload = serialize_string(message);
    
    std::cout << "[Client] Calling echo service with: " << message << std::endl;
    
    // 调用服务
    auto response = client.call("echo", payload);
    
    // 处理响应
    if (response && response->error_code == ErrorCode::Success) {
        std::string result = deserialize_string(response->payload);
        std::cout << "[Client] Success! Received: " << result << std::endl;
    } else if (response) {
        std::cout << "[Client] Error: " << response->error_message << std::endl;
    } else {
        std::cout << "[Client] Network error" << std::endl;
    }
}

/**
 * @brief 示例 2: 调用计算服务
 */
void example_computation(RpcClient& client) {
    std::cout << "\n=== Example 2: Computation Service ===" << std::endl;
    
    // 准备参数：两个整数
    int a = 42;
    int b = 58;
    
    ByteBuffer payload;
    ByteBuffer a_bytes = serialize_int(a);
    ByteBuffer b_bytes = serialize_int(b);
    payload.insert(payload.end(), a_bytes.begin(), a_bytes.end());
    payload.insert(payload.end(), b_bytes.begin(), b_bytes.end());
    
    std::cout << "[Client] Calling add service with: " << a << " + " << b << std::endl;
    
    // 调用服务
    auto response = client.call("add", payload);
    
    // 处理响应
    if (response && response->error_code == ErrorCode::Success) {
        int result = deserialize_int(response->payload);
        std::cout << "[Client] Success! Result: " << result << std::endl;
    } else if (response) {
        std::cout << "[Client] Error: " << response->error_message << std::endl;
    } else {
        std::cout << "[Client] Network error" << std::endl;
    }
}

/**
 * @brief 示例 3: 带元数据的调用
 */
void example_with_metadata(RpcClient& client) {
    std::cout << "\n=== Example 3: Call with Metadata ===" << std::endl;
    
    // 准备数据
    std::string name = "Alice";
    ByteBuffer payload = serialize_string(name);
    
    // 准备元数据
    std::unordered_map<std::string, std::string> metadata;
    metadata["trace_id"] = "trace-12345";
    metadata["user_id"] = "user-67890";
    metadata["client_version"] = "1.0.0";
    
    std::cout << "[Client] Calling greet service with metadata" << std::endl;
    std::cout << "[Client] Metadata: trace_id=" << metadata["trace_id"] << std::endl;
    
    // 调用服务
    auto response = client.call_with_metadata("greet", payload, metadata);
    
    // 处理响应
    if (response && response->error_code == ErrorCode::Success) {
        std::string result = deserialize_string(response->payload);
        std::cout << "[Client] Success! Received: " << result << std::endl;
    } else if (response) {
        std::cout << "[Client] Error: " << response->error_message << std::endl;
    } else {
        std::cout << "[Client] Network error" << std::endl;
    }
}

/**
 * @brief 示例 4: 错误处理
 */
void example_error_handling(RpcClient& client) {
    std::cout << "\n=== Example 4: Error Handling ===" << std::endl;
    
    // 场景 1: 调用不存在的服务
    std::cout << "[Client] Calling non-existent service..." << std::endl;
    auto response1 = client.call("non_existent_service", {});
    
    if (response1) {
        std::cout << "[Client] Error code: " << static_cast<int>(response1->error_code) << std::endl;
        std::cout << "[Client] Error message: " << response1->error_message << std::endl;
    }
    
    // 场景 2: 调用会返回错误的服务
    std::cout << "\n[Client] Calling error service..." << std::endl;
    auto response2 = client.call("error_service", {});
    
    if (response2) {
        std::cout << "[Client] Error code: " << static_cast<int>(response2->error_code) << std::endl;
        std::cout << "[Client] Error message: " << response2->error_message << std::endl;
    }
}

/**
 * @brief 示例 5: 多次调用
 */
void example_multiple_calls(RpcClient& client) {
    std::cout << "\n=== Example 5: Multiple Calls ===" << std::endl;
    
    // 执行多次调用
    for (int i = 1; i <= 3; ++i) {
        std::cout << "\n[Client] Call #" << i << std::endl;
        
        std::string message = "Message " + std::to_string(i);
        ByteBuffer payload = serialize_string(message);
        
        auto response = client.call("echo", payload);
        
        if (response && response->error_code == ErrorCode::Success) {
            std::string result = deserialize_string(response->payload);
            std::cout << "[Client] Request ID: " << response->request_id << std::endl;
            std::cout << "[Client] Received: " << result << std::endl;
        }
    }
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << "FRPC Client Example" << std::endl;
    std::cout << "===================" << std::endl;
    
    // 创建服务端
    RpcServer server;
    setup_example_services(server);
    
    // 创建客户端
    // 使用 lambda 函数模拟网络传输，实际上直接调用本地服务端
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    // 运行示例
    example_basic_call(client);
    example_computation(client);
    example_with_metadata(client);
    example_error_handling(client);
    example_multiple_calls(client);
    
    std::cout << "\n=== All Examples Completed ===" << std::endl;
    
    return 0;
}
