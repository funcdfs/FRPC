#include "frpc/rpc_server.h"
#include "frpc/serializer.h"
#include <iostream>
#include <cstring>

using namespace frpc;

/**
 * @brief Example: Simple Calculator Service
 * 
 * This example demonstrates how to use the RpcServer to create
 * a simple calculator service with add and multiply operations.
 */

// Helper function to extract two integers from payload
std::pair<int32_t, int32_t> extract_two_ints(const std::vector<uint8_t>& payload) {
    if (payload.size() < 8) {
        throw std::runtime_error("Payload too small for two integers");
    }
    
    int32_t a, b;
    std::memcpy(&a, payload.data(), sizeof(int32_t));
    std::memcpy(&b, payload.data() + sizeof(int32_t), sizeof(int32_t));
    return {a, b};
}

// Helper function to create payload from integer
std::vector<uint8_t> int_to_payload(int32_t value) {
    std::vector<uint8_t> payload(sizeof(int32_t));
    std::memcpy(payload.data(), &value, sizeof(int32_t));
    return payload;
}

int main() {
    std::cout << "=== FRPC Server Example: Calculator Service ===" << std::endl;
    
    // Create the RPC server
    RpcServer server;
    BinarySerializer serializer;
    
    // Register the "add" service
    server.register_service("add", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        
        try {
            auto [a, b] = extract_two_ints(req.payload);
            int32_t result = a + b;
            
            resp.error_code = ErrorCode::Success;
            resp.error_message = "";
            resp.payload = int_to_payload(result);
            
            std::cout << "  [add] " << a << " + " << b << " = " << result << std::endl;
        } catch (const std::exception& e) {
            resp.error_code = ErrorCode::InvalidArgument;
            resp.error_message = e.what();
        }
        
        return resp;
    });
    
    // Register the "multiply" service
    server.register_service("multiply", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        
        try {
            auto [a, b] = extract_two_ints(req.payload);
            int32_t result = a * b;
            
            resp.error_code = ErrorCode::Success;
            resp.error_message = "";
            resp.payload = int_to_payload(result);
            
            std::cout << "  [multiply] " << a << " * " << b << " = " << result << std::endl;
        } catch (const std::exception& e) {
            resp.error_code = ErrorCode::InvalidArgument;
            resp.error_message = e.what();
        }
        
        return resp;
    });
    
    // Register the "echo" service
    server.register_service("echo", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        resp.payload = req.payload;
        
        std::cout << "  [echo] Echoing " << req.payload.size() << " bytes" << std::endl;
        
        return resp;
    });
    
    std::cout << "\nRegistered " << server.service_count() << " services: add, multiply, echo\n" << std::endl;
    
    // Example 1: Call the "add" service
    std::cout << "Example 1: Calling 'add' service with 10 + 20" << std::endl;
    {
        Request request;
        request.request_id = 1;
        request.service_name = "add";
        request.timeout = std::chrono::milliseconds(5000);
        
        // Create payload with two integers
        int32_t a = 10, b = 20;
        request.payload.resize(sizeof(int32_t) * 2);
        std::memcpy(request.payload.data(), &a, sizeof(int32_t));
        std::memcpy(request.payload.data() + sizeof(int32_t), &b, sizeof(int32_t));
        
        // Serialize and handle
        auto request_bytes = serializer.serialize(request);
        auto response_bytes = server.handle_raw_request(request_bytes);
        auto response = serializer.deserialize_response(response_bytes);
        
        if (response.error_code == ErrorCode::Success) {
            int32_t result;
            std::memcpy(&result, response.payload.data(), sizeof(int32_t));
            std::cout << "  Result: " << result << std::endl;
        } else {
            std::cout << "  Error: " << response.error_message << std::endl;
        }
    }
    
    // Example 2: Call the "multiply" service
    std::cout << "\nExample 2: Calling 'multiply' service with 7 * 8" << std::endl;
    {
        Request request;
        request.request_id = 2;
        request.service_name = "multiply";
        request.timeout = std::chrono::milliseconds(5000);
        
        int32_t a = 7, b = 8;
        request.payload.resize(sizeof(int32_t) * 2);
        std::memcpy(request.payload.data(), &a, sizeof(int32_t));
        std::memcpy(request.payload.data() + sizeof(int32_t), &b, sizeof(int32_t));
        
        auto request_bytes = serializer.serialize(request);
        auto response_bytes = server.handle_raw_request(request_bytes);
        auto response = serializer.deserialize_response(response_bytes);
        
        if (response.error_code == ErrorCode::Success) {
            int32_t result;
            std::memcpy(&result, response.payload.data(), sizeof(int32_t));
            std::cout << "  Result: " << result << std::endl;
        } else {
            std::cout << "  Error: " << response.error_message << std::endl;
        }
    }
    
    // Example 3: Call the "echo" service
    std::cout << "\nExample 3: Calling 'echo' service" << std::endl;
    {
        Request request;
        request.request_id = 3;
        request.service_name = "echo";
        request.timeout = std::chrono::milliseconds(5000);
        request.payload = {72, 101, 108, 108, 111}; // "Hello"
        
        auto request_bytes = serializer.serialize(request);
        auto response_bytes = server.handle_raw_request(request_bytes);
        auto response = serializer.deserialize_response(response_bytes);
        
        if (response.error_code == ErrorCode::Success) {
            std::string echoed(response.payload.begin(), response.payload.end());
            std::cout << "  Echoed: " << echoed << std::endl;
        } else {
            std::cout << "  Error: " << response.error_message << std::endl;
        }
    }
    
    // Example 4: Call a non-existent service
    std::cout << "\nExample 4: Calling non-existent 'divide' service" << std::endl;
    {
        Request request;
        request.request_id = 4;
        request.service_name = "divide";
        request.timeout = std::chrono::milliseconds(5000);
        request.payload = {1, 2, 3, 4};
        
        auto request_bytes = serializer.serialize(request);
        auto response_bytes = server.handle_raw_request(request_bytes);
        auto response = serializer.deserialize_response(response_bytes);
        
        std::cout << "  Error Code: " << static_cast<int>(response.error_code) << std::endl;
        std::cout << "  Error Message: " << response.error_message << std::endl;
    }
    
    // Example 5: Send invalid request bytes
    std::cout << "\nExample 5: Sending invalid request bytes" << std::endl;
    {
        std::vector<uint8_t> invalid_bytes = {0xFF, 0xFF, 0xFF, 0xFF};
        auto response_bytes = server.handle_raw_request(invalid_bytes);
        auto response = serializer.deserialize_response(response_bytes);
        
        std::cout << "  Error Code: " << static_cast<int>(response.error_code) << std::endl;
        std::cout << "  Error Message: " << response.error_message << std::endl;
    }
    
    std::cout << "\n=== Example Complete ===" << std::endl;
    
    return 0;
}
