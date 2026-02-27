/**
 * @file benchmark_serialization.cpp
 * @brief Performance benchmarks for serialization/deserialization
 * 
 * This benchmark measures the performance of Request and Response serialization
 * and deserialization operations. The goal is to ensure efficient data conversion
 * for network transmission.
 * 
 * Performance targets:
 * - Serialization should be fast enough to not become a bottleneck
 * - Deserialization should handle high throughput scenarios
 * 
 * Requirements: 11.5, 11.6, 15.1
 */

#include <benchmark/benchmark.h>
#include "frpc/serializer.h"
#include "frpc/data_models.h"
#include <vector>
#include <string>

using namespace frpc;

/**
 * @brief Benchmark Request serialization performance
 * 
 * Measures the time taken to serialize a Request object with varying payload sizes.
 * This is critical for understanding the overhead of preparing data for network transmission.
 */
static void BM_RequestSerialization(benchmark::State& state) {
    BinarySerializer serializer;
    
    // Create a sample request with payload size from benchmark parameter
    Request request;
    request.request_id = 12345;
    request.service_name = "test_service";
    request.payload.resize(state.range(0));  // Payload size varies
    request.timeout = std::chrono::milliseconds(5000);
    request.metadata["trace_id"] = "trace-123";
    
    // Fill payload with sample data
    for (size_t i = 0; i < request.payload.size(); ++i) {
        request.payload[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Benchmark loop
    for (auto _ : state) {
        auto bytes = serializer.serialize(request);
        benchmark::DoNotOptimize(bytes);  // Prevent optimization
    }
    
    // Report throughput in bytes per second
    state.SetBytesProcessed(state.iterations() * state.range(0));
}

/**
 * @brief Benchmark Request deserialization performance
 * 
 * Measures the time taken to deserialize a Request object from byte stream.
 * This is critical for server-side request processing performance.
 */
static void BM_RequestDeserialization(benchmark::State& state) {
    BinarySerializer serializer;
    
    // Prepare serialized data
    Request request;
    request.request_id = 12345;
    request.service_name = "test_service";
    request.payload.resize(state.range(0));
    request.timeout = std::chrono::milliseconds(5000);
    
    for (size_t i = 0; i < request.payload.size(); ++i) {
        request.payload[i] = static_cast<uint8_t>(i % 256);
    }
    
    auto bytes = serializer.serialize(request);
    
    // Benchmark loop
    for (auto _ : state) {
        auto deserialized = serializer.deserialize_request(bytes);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0));
}

/**
 * @brief Benchmark Response serialization performance
 * 
 * Measures the time taken to serialize a Response object.
 * This affects server-side response preparation performance.
 */
static void BM_ResponseSerialization(benchmark::State& state) {
    BinarySerializer serializer;
    
    // Create a sample response
    Response response;
    response.request_id = 12345;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload.resize(state.range(0));
    response.metadata["server_id"] = "server-1";
    
    for (size_t i = 0; i < response.payload.size(); ++i) {
        response.payload[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Benchmark loop
    for (auto _ : state) {
        auto bytes = serializer.serialize(response);
        benchmark::DoNotOptimize(bytes);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0));
}

/**
 * @brief Benchmark Response deserialization performance
 * 
 * Measures the time taken to deserialize a Response object.
 * This affects client-side response processing performance.
 */
static void BM_ResponseDeserialization(benchmark::State& state) {
    BinarySerializer serializer;
    
    // Prepare serialized data
    Response response;
    response.request_id = 12345;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload.resize(state.range(0));
    
    for (size_t i = 0; i < response.payload.size(); ++i) {
        response.payload[i] = static_cast<uint8_t>(i % 256);
    }
    
    auto bytes = serializer.serialize(response);
    
    // Benchmark loop
    for (auto _ : state) {
        auto deserialized = serializer.deserialize_response(bytes);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0));
}

/**
 * @brief Benchmark round-trip serialization (serialize + deserialize)
 * 
 * Measures the complete cycle of serializing and deserializing a Request.
 * This represents the real-world overhead of data conversion.
 */
static void BM_RequestRoundTrip(benchmark::State& state) {
    BinarySerializer serializer;
    
    Request request;
    request.request_id = 12345;
    request.service_name = "test_service";
    request.payload.resize(state.range(0));
    request.timeout = std::chrono::milliseconds(5000);
    
    for (size_t i = 0; i < request.payload.size(); ++i) {
        request.payload[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Benchmark loop
    for (auto _ : state) {
        auto bytes = serializer.serialize(request);
        auto deserialized = serializer.deserialize_request(bytes);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * 2);  // Both directions
}

// Register benchmarks with different payload sizes
// Small payloads (typical for control messages)
BENCHMARK(BM_RequestSerialization)->Range(64, 1024);
BENCHMARK(BM_RequestDeserialization)->Range(64, 1024);
BENCHMARK(BM_ResponseSerialization)->Range(64, 1024);
BENCHMARK(BM_ResponseDeserialization)->Range(64, 1024);

// Medium payloads (typical for data transfer)
BENCHMARK(BM_RequestSerialization)->Range(1024, 64*1024);
BENCHMARK(BM_RequestDeserialization)->Range(1024, 64*1024);
BENCHMARK(BM_ResponseSerialization)->Range(1024, 64*1024);
BENCHMARK(BM_ResponseDeserialization)->Range(1024, 64*1024);

// Round-trip benchmarks
BENCHMARK(BM_RequestRoundTrip)->Range(64, 64*1024);

// Run the benchmarks
BENCHMARK_MAIN();
