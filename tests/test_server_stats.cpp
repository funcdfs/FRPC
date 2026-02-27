/**
 * @file test_server_stats.cpp
 * @brief Unit tests for RPC server statistics collection
 * 
 * Tests the ServerStats structure and statistics collection functionality
 * in the RPC server, including request counting, latency tracking, and QPS calculation.
 */

#include <gtest/gtest.h>
#include "frpc/rpc_server.h"
#include "frpc/data_models.h"
#include "frpc/serializer.h"
#include <thread>
#include <chrono>

using namespace frpc;

/**
 * @brief Test fixture for server statistics tests
 */
class ServerStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        server = std::make_unique<RpcServer>();
        serializer = std::make_unique<BinarySerializer>();
        
        // Register a simple echo service
        server->register_service("echo", [](const Request& req) {
            return Response{req.request_id, ErrorCode::Success, "", req.payload};
        });
        
        // Register a service that always fails
        server->register_service("fail", [](const Request& req) {
            return Response{req.request_id, ErrorCode::ServiceException, "Intentional failure", {}};
        });
    }

    std::unique_ptr<RpcServer> server;
    std::unique_ptr<BinarySerializer> serializer;
};

/**
 * @brief Test initial statistics are zero
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, InitialStatsAreZero) {
    auto stats = server->get_stats();
    
    EXPECT_EQ(stats.total_requests, 0);
    EXPECT_EQ(stats.successful_requests, 0);
    EXPECT_EQ(stats.failed_requests, 0);
    EXPECT_EQ(stats.active_connections, 0);
    EXPECT_EQ(stats.avg_latency_ms, 0.0);
    EXPECT_EQ(stats.p99_latency_ms, 0.0);
    EXPECT_EQ(stats.qps, 0.0);
}

/**
 * @brief Test successful request counting
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, SuccessfulRequestCounting) {
    // Create a valid request
    Request request;
    request.request_id = 1;
    request.service_name = "echo";
    request.payload = {1, 2, 3, 4, 5};
    
    // Serialize and handle the request
    auto request_bytes = serializer->serialize(request);
    auto response_bytes = server->handle_raw_request(request_bytes);
    
    // Check statistics
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, 1);
    EXPECT_EQ(stats.successful_requests, 1);
    EXPECT_EQ(stats.failed_requests, 0);
}

/**
 * @brief Test failed request counting
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, FailedRequestCounting) {
    // Create a request for a service that fails
    Request request;
    request.request_id = 1;
    request.service_name = "fail";
    request.payload = {};
    
    // Serialize and handle the request
    auto request_bytes = serializer->serialize(request);
    auto response_bytes = server->handle_raw_request(request_bytes);
    
    // Check statistics
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, 1);
    EXPECT_EQ(stats.successful_requests, 0);
    EXPECT_EQ(stats.failed_requests, 1);
}

/**
 * @brief Test service not found counting
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, ServiceNotFoundCounting) {
    // Create a request for a non-existent service
    Request request;
    request.request_id = 1;
    request.service_name = "nonexistent";
    request.payload = {};
    
    // Serialize and handle the request
    auto request_bytes = serializer->serialize(request);
    auto response_bytes = server->handle_raw_request(request_bytes);
    
    // Check statistics
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, 1);
    EXPECT_EQ(stats.successful_requests, 0);
    EXPECT_EQ(stats.failed_requests, 1);
}

/**
 * @brief Test deserialization error counting
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, DeserializationErrorCounting) {
    // Create invalid request bytes
    std::vector<uint8_t> invalid_bytes = {0xFF, 0xFF, 0xFF, 0xFF};
    
    // Handle the invalid request
    auto response_bytes = server->handle_raw_request(invalid_bytes);
    
    // Check statistics
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, 1);
    EXPECT_EQ(stats.successful_requests, 0);
    EXPECT_EQ(stats.failed_requests, 1);
}

/**
 * @brief Test multiple requests counting
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, MultipleRequestsCounting) {
    // Process multiple successful requests
    for (int i = 0; i < 5; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "echo";
        request.payload = {static_cast<uint8_t>(i)};
        
        auto request_bytes = serializer->serialize(request);
        server->handle_raw_request(request_bytes);
    }
    
    // Process multiple failed requests
    for (int i = 0; i < 3; ++i) {
        Request request;
        request.request_id = i + 5;
        request.service_name = "fail";
        request.payload = {};
        
        auto request_bytes = serializer->serialize(request);
        server->handle_raw_request(request_bytes);
    }
    
    // Check statistics
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, 8);
    EXPECT_EQ(stats.successful_requests, 5);
    EXPECT_EQ(stats.failed_requests, 3);
}

/**
 * @brief Test latency tracking
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, LatencyTracking) {
    // Process a request
    Request request;
    request.request_id = 1;
    request.service_name = "echo";
    request.payload = {1, 2, 3};
    
    auto request_bytes = serializer->serialize(request);
    server->handle_raw_request(request_bytes);
    
    // Check that latency is tracked
    auto stats = server->get_stats();
    EXPECT_GT(stats.avg_latency_ms, 0.0);
    EXPECT_GT(stats.p99_latency_ms, 0.0);
}

/**
 * @brief Test P99 latency calculation
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, P99LatencyCalculation) {
    // Process multiple requests to get meaningful P99
    for (int i = 0; i < 100; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "echo";
        request.payload = {static_cast<uint8_t>(i)};
        
        auto request_bytes = serializer->serialize(request);
        server->handle_raw_request(request_bytes);
        
        // Add small delay to create latency variation
        if (i % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    // Check statistics
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, 100);
    EXPECT_GT(stats.avg_latency_ms, 0.0);
    EXPECT_GT(stats.p99_latency_ms, 0.0);
    
    // P99 should be >= average (since it's a high percentile)
    EXPECT_GE(stats.p99_latency_ms, stats.avg_latency_ms);
}

/**
 * @brief Test QPS calculation
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, QPSCalculation) {
    // Process requests over a short time period
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 10; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "echo";
        request.payload = {static_cast<uint8_t>(i)};
        
        auto request_bytes = serializer->serialize(request);
        server->handle_raw_request(request_bytes);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration_seconds = std::chrono::duration<double>(end - start).count();
    
    // Check QPS
    auto stats = server->get_stats();
    EXPECT_GT(stats.qps, 0.0);
    
    // QPS should be approximately requests / duration
    double expected_qps = 10.0 / duration_seconds;
    
    // Allow for some variance (within 50% of expected)
    EXPECT_GT(stats.qps, expected_qps * 0.5);
    EXPECT_LT(stats.qps, expected_qps * 1.5);
}

/**
 * @brief Test active connections (always 0 in MVP)
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, ActiveConnectionsAlwaysZero) {
    // Process some requests
    for (int i = 0; i < 5; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "echo";
        request.payload = {};
        
        auto request_bytes = serializer->serialize(request);
        server->handle_raw_request(request_bytes);
    }
    
    // Active connections should always be 0 in MVP
    auto stats = server->get_stats();
    EXPECT_EQ(stats.active_connections, 0);
}

/**
 * @brief Test statistics consistency
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, StatisticsConsistency) {
    // Process mixed requests
    for (int i = 0; i < 20; ++i) {
        Request request;
        request.request_id = i;
        
        // Alternate between success and failure
        if (i % 2 == 0) {
            request.service_name = "echo";
        } else {
            request.service_name = "fail";
        }
        
        request.payload = {};
        
        auto request_bytes = serializer->serialize(request);
        server->handle_raw_request(request_bytes);
    }
    
    // Check consistency: total = successful + failed
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, stats.successful_requests + stats.failed_requests);
    EXPECT_EQ(stats.total_requests, 20);
    EXPECT_EQ(stats.successful_requests, 10);
    EXPECT_EQ(stats.failed_requests, 10);
}

/**
 * @brief Test statistics after many requests
 * 
 * Validates: Requirements 11.4, 15.1
 */
TEST_F(ServerStatsTest, StatisticsAfterManyRequests) {
    // Process many requests to test sample limiting
    for (int i = 0; i < 2000; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "echo";
        request.payload = {static_cast<uint8_t>(i % 256)};
        
        auto request_bytes = serializer->serialize(request);
        server->handle_raw_request(request_bytes);
    }
    
    // Check statistics
    auto stats = server->get_stats();
    EXPECT_EQ(stats.total_requests, 2000);
    EXPECT_EQ(stats.successful_requests, 2000);
    EXPECT_EQ(stats.failed_requests, 0);
    
    // Latency and QPS should still be calculated correctly
    EXPECT_GT(stats.avg_latency_ms, 0.0);
    EXPECT_GT(stats.p99_latency_ms, 0.0);
    EXPECT_GT(stats.qps, 0.0);
}

