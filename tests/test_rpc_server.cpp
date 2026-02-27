#include <gtest/gtest.h>
#include "frpc/rpc_server.h"
#include "frpc/serializer.h"
#include <cstring>

using namespace frpc;

class RpcServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server = std::make_unique<RpcServer>();
        serializer = std::make_unique<BinarySerializer>();
    }

    std::unique_ptr<RpcServer> server;
    std::unique_ptr<BinarySerializer> serializer;
};

// Test 4.1: Service registration interface
TEST_F(RpcServerTest, ServiceRegistration) {
    // Register a simple echo service
    bool registered = server->register_service("echo", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.payload = req.payload;
        return resp;
    });
    
    EXPECT_TRUE(registered);
    EXPECT_TRUE(server->has_service("echo"));
    EXPECT_EQ(server->service_count(), 1);
}

TEST_F(RpcServerTest, DuplicateServiceRegistration) {
    // Register a service
    server->register_service("test", [](const Request& req) {
        return Response{req.request_id, ErrorCode::Success, "", {}};
    });
    
    // Try to register the same service again
    bool registered = server->register_service("test", [](const Request& req) {
        return Response{req.request_id, ErrorCode::Success, "", {}};
    });
    
    EXPECT_FALSE(registered);
    EXPECT_EQ(server->service_count(), 1);
}

TEST_F(RpcServerTest, ServiceUnregistration) {
    // Register a service
    server->register_service("test", [](const Request& req) {
        return Response{req.request_id, ErrorCode::Success, "", {}};
    });
    
    EXPECT_TRUE(server->has_service("test"));
    
    // Unregister it
    bool unregistered = server->unregister_service("test");
    EXPECT_TRUE(unregistered);
    EXPECT_FALSE(server->has_service("test"));
    EXPECT_EQ(server->service_count(), 0);
}

TEST_F(RpcServerTest, UnregisterNonexistentService) {
    bool unregistered = server->unregister_service("nonexistent");
    EXPECT_FALSE(unregistered);
}

// Test 4.2: Request deserialization (using BinarySerializer)
TEST_F(RpcServerTest, RequestDeserialization) {
    // Register a service that echoes the service name
    server->register_service("test_service", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "";
        // Echo the service name in the payload
        resp.payload.assign(req.service_name.begin(), req.service_name.end());
        return resp;
    });
    
    // Create a request
    Request request;
    request.request_id = 42;
    request.service_name = "test_service";
    request.payload = {1, 2, 3, 4, 5};
    request.timeout = std::chrono::milliseconds(5000);
    
    // Serialize the request
    auto request_bytes = serializer->serialize(request);
    
    // Handle the request
    auto response_bytes = server->handle_raw_request(request_bytes);
    
    // Deserialize the response
    auto response = serializer->deserialize_response(response_bytes);
    
    // Verify the response
    EXPECT_EQ(response.request_id, 42);
    EXPECT_EQ(response.error_code, ErrorCode::Success);
    EXPECT_EQ(response.error_message, "");
    
    // The payload should contain the service name
    std::string service_name_from_payload(response.payload.begin(), response.payload.end());
    EXPECT_EQ(service_name_from_payload, "test_service");
}

// Test 4.3: Service routing
TEST_F(RpcServerTest, ServiceRouting) {
    // Register multiple services
    server->register_service("add", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.payload = {10}; // Dummy result
        return resp;
    });
    
    server->register_service("multiply", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.payload = {20}; // Dummy result
        return resp;
    });
    
    // Create requests for different services
    Request add_request{1, "add", {}, std::chrono::milliseconds(5000), {}};
    Request multiply_request{2, "multiply", {}, std::chrono::milliseconds(5000), {}};
    
    // Handle the requests
    auto add_response = server->handle_request(add_request);
    auto multiply_response = server->handle_request(multiply_request);
    
    // Verify routing worked correctly
    EXPECT_EQ(add_response.request_id, 1);
    EXPECT_EQ(add_response.error_code, ErrorCode::Success);
    EXPECT_EQ(add_response.payload[0], 10);
    
    EXPECT_EQ(multiply_response.request_id, 2);
    EXPECT_EQ(multiply_response.error_code, ErrorCode::Success);
    EXPECT_EQ(multiply_response.payload[0], 20);
}

// Test 4.4: Response serialization
TEST_F(RpcServerTest, ResponseSerialization) {
    // Register a service
    server->register_service("test", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.error_message = "OK";
        resp.payload = {1, 2, 3};
        return resp;
    });
    
    // Create and serialize a request
    Request request{123, "test", {}, std::chrono::milliseconds(5000), {}};
    auto request_bytes = serializer->serialize(request);
    
    // Handle the request (returns serialized response)
    auto response_bytes = server->handle_raw_request(request_bytes);
    
    // Deserialize and verify
    auto response = serializer->deserialize_response(response_bytes);
    EXPECT_EQ(response.request_id, 123);
    EXPECT_EQ(response.error_code, ErrorCode::Success);
    EXPECT_EQ(response.error_message, "OK");
    EXPECT_EQ(response.payload, std::vector<uint8_t>({1, 2, 3}));
}

// Test 4.5: Handle deserialization failures
TEST_F(RpcServerTest, DeserializationFailure) {
    // Create invalid request bytes
    std::vector<uint8_t> invalid_bytes = {0xFF, 0xFF, 0xFF, 0xFF};
    
    // Handle the invalid request
    auto response_bytes = server->handle_raw_request(invalid_bytes);
    
    // Deserialize the error response
    auto response = serializer->deserialize_response(response_bytes);
    
    // Verify error response
    EXPECT_EQ(response.error_code, ErrorCode::DeserializationError);
    EXPECT_FALSE(response.error_message.empty());
    EXPECT_TRUE(response.error_message.find("deserialize") != std::string::npos ||
                response.error_message.find("Deserialize") != std::string::npos);
}

// Test 4.6: Handle service not found errors
TEST_F(RpcServerTest, ServiceNotFound) {
    // Don't register any services
    
    // Create a request for a non-existent service
    Request request{456, "nonexistent_service", {}, std::chrono::milliseconds(5000), {}};
    
    // Handle the request
    auto response = server->handle_request(request);
    
    // Verify error response
    EXPECT_EQ(response.request_id, 456);
    EXPECT_EQ(response.error_code, ErrorCode::ServiceNotFound);
    EXPECT_FALSE(response.error_message.empty());
    EXPECT_TRUE(response.error_message.find("not found") != std::string::npos ||
                response.error_message.find("Not found") != std::string::npos);
}

// Test service exception handling
TEST_F(RpcServerTest, ServiceExceptionHandling) {
    // Register a service that throws an exception
    server->register_service("throwing_service", [](const Request& req) -> Response {
        throw std::runtime_error("Service error");
    });
    
    // Create a request
    Request request{789, "throwing_service", {}, std::chrono::milliseconds(5000), {}};
    
    // Handle the request
    auto response = server->handle_request(request);
    
    // Verify error response
    EXPECT_EQ(response.request_id, 789);
    EXPECT_EQ(response.error_code, ErrorCode::ServiceException);
    EXPECT_FALSE(response.error_message.empty());
}

// Test with empty service name
TEST_F(RpcServerTest, EmptyServiceName) {
    Request request{100, "", {}, std::chrono::milliseconds(5000), {}};
    
    auto response = server->handle_request(request);
    
    EXPECT_EQ(response.request_id, 100);
    EXPECT_EQ(response.error_code, ErrorCode::ServiceNotFound);
}

// Test with large payload
TEST_F(RpcServerTest, LargePayload) {
    // Register an echo service
    server->register_service("echo", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.payload = req.payload;
        return resp;
    });
    
    // Create a request with a large payload
    std::vector<uint8_t> large_payload(10000, 0xAB);
    Request request{200, "echo", large_payload, std::chrono::milliseconds(5000), {}};
    
    auto request_bytes = serializer->serialize(request);
    auto response_bytes = server->handle_raw_request(request_bytes);
    auto response = serializer->deserialize_response(response_bytes);
    
    EXPECT_EQ(response.request_id, 200);
    EXPECT_EQ(response.error_code, ErrorCode::Success);
    EXPECT_EQ(response.payload.size(), 10000);
    EXPECT_EQ(response.payload, large_payload);
}

// Test multiple services
TEST_F(RpcServerTest, MultipleServices) {
    // Register multiple services
    for (int i = 0; i < 10; ++i) {
        std::string service_name = "service_" + std::to_string(i);
        server->register_service(service_name, [i](const Request& req) {
            Response resp;
            resp.request_id = req.request_id;
            resp.error_code = ErrorCode::Success;
            resp.payload = {static_cast<uint8_t>(i)};
            return resp;
        });
    }
    
    EXPECT_EQ(server->service_count(), 10);
    
    // Test each service
    for (int i = 0; i < 10; ++i) {
        std::string service_name = "service_" + std::to_string(i);
        Request request{static_cast<uint32_t>(i), service_name, {}, std::chrono::milliseconds(5000), {}};
        auto response = server->handle_request(request);
        
        EXPECT_EQ(response.error_code, ErrorCode::Success);
        EXPECT_EQ(response.payload[0], i);
    }
}

// Test request ID preservation
TEST_F(RpcServerTest, RequestIdPreservation) {
    server->register_service("test", [](const Request& req) {
        return Response{req.request_id, ErrorCode::Success, "", {}};
    });
    
    // Test with various request IDs
    std::vector<uint32_t> request_ids = {0, 1, 100, 999999, UINT32_MAX};
    
    for (uint32_t id : request_ids) {
        Request request{id, "test", {}, std::chrono::milliseconds(5000), {}};
        auto response = server->handle_request(request);
        EXPECT_EQ(response.request_id, id);
    }
}
