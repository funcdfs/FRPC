#pragma once

#include "data_models.h"
#include "serializer.h"
#include "error_codes.h"
#include "exceptions.h"
#include <functional>
#include <unordered_map>
#include <string>
#include <memory>
#include <optional>
#include <atomic>
#include <mutex>
#include <vector>
#include <deque>
#include <chrono>

namespace frpc {

/**
 * @brief Service handler function type
 * 
 * A service handler takes a Request and returns a Response.
 * For the MVP, handlers are simple synchronous functions.
 */
using ServiceHandler = std::function<Response(const Request&)>;

/**
 * @brief Simplified MVP RPC Server
 * 
 * This is a minimal but functional RPC server that demonstrates the core
 * RPC logic without the complexity of network engines or coroutines.
 * 
 * Features:
 * - Service registration (register handlers by name)
 * - Request routing (route requests to correct handler)
 * - Request deserialization
 * - Response serialization
 * - Error handling (deserialization failures, service not found)
 * 
 * Usage example:
 * @code
 * RpcServer server;
 * 
 * // Register a service
 * server.register_service("add", [](const Request& req) {
 *     // Parse payload, do work, return response
 *     return Response{req.request_id, ErrorCode::Success, "", result_payload};
 * });
 * 
 * // Handle a request
 * std::vector<uint8_t> request_bytes = ...;
 * auto response_bytes = server.handle_raw_request(request_bytes);
 * @endcode
 * 
 * @note This is an MVP implementation. Full implementation would use:
 * - NetworkEngine for async I/O
 * - Coroutines for concurrent request handling
 * - Connection management
 */
class RpcServer {
public:
    /**
     * @brief Construct a new RPC Server
     */
    RpcServer();

    /**
     * @brief Destroy the RPC Server
     */
    ~RpcServer();

    /**
     * @brief Register a service handler
     * 
     * @param service_name The name of the service
     * @param handler The handler function to invoke for this service
     * @return true if registration succeeded
     * @return false if a service with this name already exists
     * 
     * @example
     * server.register_service("echo", [](const Request& req) {
     *     return Response{req.request_id, ErrorCode::Success, "", req.payload};
     * });
     */
    bool register_service(const std::string& service_name, ServiceHandler handler);

    /**
     * @brief Unregister a service handler
     * 
     * @param service_name The name of the service to remove
     * @return true if the service was found and removed
     * @return false if the service was not found
     */
    bool unregister_service(const std::string& service_name);

    /**
     * @brief Check if a service is registered
     * 
     * @param service_name The name of the service
     * @return true if the service is registered
     * @return false otherwise
     */
    bool has_service(const std::string& service_name) const;

    /**
     * @brief Handle a raw request (bytes)
     * 
     * This is the main entry point for processing requests.
     * It performs the following steps:
     * 1. Deserialize the request bytes
     * 2. Route to the appropriate handler
     * 3. Execute the handler
     * 4. Serialize the response
     * 5. Handle any errors that occur
     * 
     * @param request_bytes The serialized request
     * @return std::vector<uint8_t> The serialized response
     * 
     * Error handling:
     * - If deserialization fails, returns an error response with DeserializationError
     *   and logs detailed error information including exception message and byte size
     * - If service not found, returns an error response with ServiceNotFound
     *   and logs a warning with the service name and request ID
     * - If handler throws, returns an error response with ServiceException
     *   and logs detailed error information including exception details
     * 
     * Logging:
     * - DEBUG: Successful deserialization with request details
     * - WARN: Service not found with service name and request ID
     * - ERROR: Deserialization failures with exception details and byte size
     * - ERROR: Service execution exceptions with full context
     * 
     * @note All errors are logged with detailed context to aid debugging.
     *       Request ID is set to 0 in error responses when deserialization fails
     *       since the actual request ID cannot be determined.
     * 
     * @see handle_request() for handling already-deserialized requests
     * @see Requirements 4.5, 4.6, 12.1, 12.3, 15.1
     */
    std::vector<uint8_t> handle_raw_request(const std::vector<uint8_t>& request_bytes);

    /**
     * @brief Handle a deserialized request
     * 
     * This method assumes the request has already been deserialized.
     * It routes the request to the appropriate handler and returns a response.
     * 
     * @param request The deserialized request
     * @return Response The response from the handler
     * 
     * Error handling:
     * - If service not found, returns error response with ServiceNotFound
     *   and logs a warning with service name and request ID
     * - If handler throws std::exception, returns error response with ServiceException
     *   and logs detailed error information including exception message
     * - If handler throws unknown exception, returns error response with ServiceException
     *   and logs error with service name and request ID
     * 
     * Logging:
     * - DEBUG: Request handling start with request details (ID, service, payload size)
     * - DEBUG: Service handler execution start
     * - DEBUG: Successful service execution with response details
     * - WARN: Service not found with service name and request ID
     * - WARN: Service returned error response with error code and message
     * - ERROR: Service execution exceptions with full exception details
     * - ERROR: Unknown exceptions with service context
     * 
     * Exception safety:
     * - All exceptions thrown by service handlers are caught and converted to error responses
     * - No exceptions propagate out of this method
     * - Resources are properly cleaned up even when exceptions occur
     * 
     * @note This method never throws exceptions. All errors are returned as Response objects
     *       with appropriate error codes and messages.
     * 
     * @see handle_raw_request() for handling raw byte requests
     * @see Requirements 4.3, 4.6, 12.1, 12.3, 15.1
     */
    Response handle_request(const Request& request);

    /**
     * @brief Get the number of registered services
     * 
     * @return size_t The number of services
     */
    size_t service_count() const;

    /**
     * @brief Get server statistics
     * 
     * Returns comprehensive statistics about the server's operation, including:
     * - Request counts (total, successful, failed)
     * - Active connections (always 0 in MVP)
     * - Latency metrics (average, P99)
     * - Throughput (QPS)
     * 
     * These statistics are useful for:
     * - Monitoring server health and performance
     * - Identifying performance bottlenecks
     * - Capacity planning
     * - Alerting on anomalies
     * 
     * @return ServerStats The current statistics snapshot
     * 
     * @note Statistics are cumulative from server start
     * @note Latency and QPS are calculated based on recent requests
     * @note Thread-safe: can be called concurrently with request handling
     * 
     * @example
     * @code
     * auto stats = server.get_stats();
     * std::cout << "Total: " << stats.total_requests << std::endl;
     * std::cout << "Success rate: " 
     *           << (100.0 * stats.successful_requests / stats.total_requests) 
     *           << "%" << std::endl;
     * std::cout << "Avg latency: " << stats.avg_latency_ms << "ms" << std::endl;
     * std::cout << "P99 latency: " << stats.p99_latency_ms << "ms" << std::endl;
     * std::cout << "QPS: " << stats.qps << std::endl;
     * @endcode
     * 
     * @see ServerStats
     * @see Requirements 11.4, 15.1
     */
    ServerStats get_stats() const;

private:
    /**
     * @brief Create an error response
     * 
     * Helper method to construct a standardized error response with the given
     * error code and message. The response will have an empty payload.
     * 
     * @param request_id The request ID (use 0 if request ID is unknown, e.g., deserialization failed)
     * @param error_code The error code indicating the type of error
     * @param error_message The human-readable error message describing what went wrong
     * @return Response The error response with specified error code and message
     * 
     * @note The returned response will have:
     *       - request_id: as specified
     *       - error_code: as specified
     *       - error_message: as specified
     *       - payload: empty vector
     * 
     * @see ErrorCode for available error codes
     * @see Requirements 12.1, 12.2
     */
    Response create_error_response(uint32_t request_id, 
                                   ErrorCode error_code, 
                                   const std::string& error_message) const;

    /**
     * @brief Record request statistics
     * 
     * Updates statistics counters and latency metrics for a completed request.
     * This method is called after each request is processed.
     * 
     * @param success Whether the request was successful (error_code == Success)
     * @param latency_ms The request processing latency in milliseconds
     * 
     * @note Thread-safe: uses atomic operations for counters
     */
    void record_request_stats(bool success, double latency_ms);

    /**
     * @brief Calculate current QPS
     * 
     * Calculates queries per second based on recent request timestamps.
     * Uses a sliding window approach to provide real-time QPS values.
     * 
     * @return double The current QPS
     */
    double calculate_qps() const;

    std::unordered_map<std::string, ServiceHandler> services_;
    std::unique_ptr<Serializer> serializer_;
    
    // Statistics tracking
    mutable std::mutex stats_mutex_;  ///< Protects statistics data structures
    std::atomic<uint64_t> total_requests_{0};      ///< Total request count
    std::atomic<uint64_t> successful_requests_{0}; ///< Successful request count
    std::atomic<uint64_t> failed_requests_{0};     ///< Failed request count
    
    // Latency tracking (protected by stats_mutex_)
    std::vector<double> latencies_;  ///< Recent request latencies for P99 calculation
    double total_latency_ms_{0.0};   ///< Sum of all latencies for average calculation
    
    // QPS tracking (protected by stats_mutex_)
    std::deque<std::chrono::steady_clock::time_point> request_timestamps_;  ///< Recent request timestamps for QPS calculation
    static constexpr size_t MAX_LATENCY_SAMPLES = 1000;  ///< Maximum latency samples to keep
    static constexpr size_t QPS_WINDOW_SECONDS = 10;     ///< QPS calculation window in seconds
};

} // namespace frpc
