/**
 * @file server_stats_example.cpp
 * @brief Example demonstrating RPC server statistics collection
 * 
 * This example shows how to:
 * - Register services on an RPC server
 * - Process multiple requests
 * - Collect and display server statistics
 * - Monitor server performance metrics
 */

#include "frpc/rpc_server.h"
#include "frpc/data_models.h"
#include "frpc/serializer.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace frpc;

/**
 * @brief Display server statistics in a formatted table
 */
void display_stats(const ServerStats& stats) {
    std::cout << "\n=== Server Statistics ===" << std::endl;
    std::cout << std::left << std::setw(30) << "Metric" << "Value" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    std::cout << std::setw(30) << "Total Requests:" 
              << stats.total_requests << std::endl;
    std::cout << std::setw(30) << "Successful Requests:" 
              << stats.successful_requests << std::endl;
    std::cout << std::setw(30) << "Failed Requests:" 
              << stats.failed_requests << std::endl;
    std::cout << std::setw(30) << "Active Connections:" 
              << stats.active_connections << std::endl;
    
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(30) << "Average Latency (ms):" 
              << stats.avg_latency_ms << std::endl;
    std::cout << std::setw(30) << "P99 Latency (ms):" 
              << stats.p99_latency_ms << std::endl;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(30) << "QPS (queries/sec):" 
              << stats.qps << std::endl;
    
    // Calculate success rate
    if (stats.total_requests > 0) {
        double success_rate = 100.0 * stats.successful_requests / stats.total_requests;
        std::cout << std::setw(30) << "Success Rate:" 
                  << success_rate << "%" << std::endl;
    }
    
    std::cout << std::string(50, '=') << std::endl;
}

int main() {
    std::cout << "FRPC Server Statistics Example\n" << std::endl;
    
    // Create server and serializer
    RpcServer server;
    BinarySerializer serializer;
    
    // Register services
    std::cout << "Registering services..." << std::endl;
    
    // 1. Echo service - always succeeds
    server.register_service("echo", [](const Request& req) {
        return Response{req.request_id, ErrorCode::Success, "", req.payload, {}};
    });
    
    // 2. Add service - simulates computation
    server.register_service("add", [](const Request& req) {
        // Simulate some processing time
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return Response{req.request_id, ErrorCode::Success, "", {42}, {}};
    });
    
    // 3. Slow service - simulates slow operation
    server.register_service("slow", [](const Request& req) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return Response{req.request_id, ErrorCode::Success, "", {}, {}};
    });
    
    // 4. Failing service - always fails
    server.register_service("fail", [](const Request& req) {
        return Response{req.request_id, ErrorCode::ServiceException, "Intentional failure", {}, {}};
    });
    
    std::cout << "Services registered: " << server.service_count() << "\n" << std::endl;
    
    // Display initial statistics
    std::cout << "Initial statistics:" << std::endl;
    display_stats(server.get_stats());
    
    // Process some requests
    std::cout << "\nProcessing requests..." << std::endl;
    
    // Process fast requests
    std::cout << "- Processing 50 echo requests..." << std::endl;
    for (int i = 0; i < 50; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "echo";
        request.payload = {static_cast<uint8_t>(i)};
        
        auto request_bytes = serializer.serialize(request);
        server.handle_raw_request(request_bytes);
    }
    
    // Process computation requests
    std::cout << "- Processing 20 add requests..." << std::endl;
    for (int i = 50; i < 70; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "add";
        request.payload = {1, 2};
        
        auto request_bytes = serializer.serialize(request);
        server.handle_raw_request(request_bytes);
    }
    
    // Process slow requests
    std::cout << "- Processing 10 slow requests..." << std::endl;
    for (int i = 70; i < 80; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "slow";
        request.payload = {};
        
        auto request_bytes = serializer.serialize(request);
        server.handle_raw_request(request_bytes);
    }
    
    // Process failing requests
    std::cout << "- Processing 10 failing requests..." << std::endl;
    for (int i = 80; i < 90; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "fail";
        request.payload = {};
        
        auto request_bytes = serializer.serialize(request);
        server.handle_raw_request(request_bytes);
    }
    
    // Process requests to non-existent service
    std::cout << "- Processing 5 requests to non-existent service..." << std::endl;
    for (int i = 90; i < 95; ++i) {
        Request request;
        request.request_id = i;
        request.service_name = "nonexistent";
        request.payload = {};
        
        auto request_bytes = serializer.serialize(request);
        server.handle_raw_request(request_bytes);
    }
    
    // Process invalid requests
    std::cout << "- Processing 5 invalid requests..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> invalid_bytes = {0xFF, 0xFF, 0xFF, 0xFF};
        server.handle_raw_request(invalid_bytes);
    }
    
    // Display final statistics
    std::cout << "\nFinal statistics:" << std::endl;
    display_stats(server.get_stats());
    
    // Demonstrate continuous monitoring
    std::cout << "\nDemonstrating continuous monitoring..." << std::endl;
    std::cout << "Processing requests in batches and monitoring stats:\n" << std::endl;
    
    for (int batch = 0; batch < 3; ++batch) {
        std::cout << "Batch " << (batch + 1) << ":" << std::endl;
        
        // Process a batch of requests
        for (int i = 0; i < 20; ++i) {
            Request request;
            request.request_id = 100 + batch * 20 + i;
            request.service_name = (i % 2 == 0) ? "echo" : "add";
            request.payload = {static_cast<uint8_t>(i)};
            
            auto request_bytes = serializer.serialize(request);
            server.handle_raw_request(request_bytes);
        }
        
        // Display current statistics
        auto stats = server.get_stats();
        std::cout << "  Total: " << stats.total_requests 
                  << ", Success: " << stats.successful_requests
                  << ", Failed: " << stats.failed_requests
                  << ", QPS: " << std::fixed << std::setprecision(2) << stats.qps
                  << ", Avg Latency: " << std::fixed << std::setprecision(3) 
                  << stats.avg_latency_ms << "ms" << std::endl;
        
        // Small delay between batches
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Final summary
    std::cout << "\n=== Final Summary ===" << std::endl;
    auto final_stats = server.get_stats();
    display_stats(final_stats);
    
    std::cout << "\nKey Observations:" << std::endl;
    std::cout << "- Total requests processed: " << final_stats.total_requests << std::endl;
    std::cout << "- Success rate: " 
              << std::fixed << std::setprecision(2)
              << (100.0 * final_stats.successful_requests / final_stats.total_requests) 
              << "%" << std::endl;
    std::cout << "- P99 latency indicates that 99% of requests completed in " 
              << std::fixed << std::setprecision(3)
              << final_stats.p99_latency_ms << "ms or less" << std::endl;
    std::cout << "- Average throughput: " 
              << std::fixed << std::setprecision(2)
              << final_stats.qps << " queries per second" << std::endl;
    
    std::cout << "\nExample completed successfully!" << std::endl;
    
    return 0;
}
