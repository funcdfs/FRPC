#include "frpc/rpc_server.h"
#include "frpc/logger.h"
#include <stdexcept>
#include <algorithm>
#include <numeric>

namespace frpc {

RpcServer::RpcServer() 
    : serializer_(std::make_unique<BinarySerializer>()) {
}

RpcServer::~RpcServer() = default;

bool RpcServer::register_service(const std::string& service_name, ServiceHandler handler) {
    // Check if service already exists
    if (services_.find(service_name) != services_.end()) {
        return false;
    }
    
    services_[service_name] = std::move(handler);
    return true;
}

bool RpcServer::unregister_service(const std::string& service_name) {
    auto it = services_.find(service_name);
    if (it == services_.end()) {
        return false;
    }
    
    services_.erase(it);
    return true;
}

bool RpcServer::has_service(const std::string& service_name) const {
    return services_.find(service_name) != services_.end();
}

std::vector<uint8_t> RpcServer::handle_raw_request(const std::vector<uint8_t>& request_bytes) {
    auto start_time = std::chrono::steady_clock::now();
    
    Request request;
    
    // Step 1: Deserialize the request
    try {
        request = serializer_->deserialize_request(request_bytes);
        LOG_DEBUG("RpcServer", "Successfully deserialized request: id={}, service={}", 
                  request.request_id, request.service_name);
    } catch (const SerializationException& e) {
        // Deserialization failed - return error response
        // We don't have a request_id, so use 0
        LOG_ERROR("RpcServer", "Failed to deserialize request: {} (bytes_size={})", 
                  e.what(), request_bytes.size());
        
        auto error_response = create_error_response(
            0,
            ErrorCode::DeserializationError,
            std::string("Failed to deserialize request: ") + e.what()
        );
        
        // Record failed request
        auto end_time = std::chrono::steady_clock::now();
        auto latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        record_request_stats(false, latency_ms);
        
        return serializer_->serialize(error_response);
    } catch (const std::exception& e) {
        // Unexpected error during deserialization
        LOG_ERROR("RpcServer", "Unexpected error while deserializing request: {} (bytes_size={})", 
                  e.what(), request_bytes.size());
        
        auto error_response = create_error_response(
            0,
            ErrorCode::DeserializationError,
            std::string("Unexpected error: failed to deserialize request: ") + e.what()
        );
        
        // Record failed request
        auto end_time = std::chrono::steady_clock::now();
        auto latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        record_request_stats(false, latency_ms);
        
        return serializer_->serialize(error_response);
    }
    
    // Step 2: Handle the request
    Response response = handle_request(request);
    
    // Step 3: Record statistics
    auto end_time = std::chrono::steady_clock::now();
    auto latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    bool success = (response.error_code == ErrorCode::Success);
    record_request_stats(success, latency_ms);
    
    // Step 4: Serialize the response
    return serializer_->serialize(response);
}

Response RpcServer::handle_request(const Request& request) {
    LOG_DEBUG("RpcServer", "Handling request: id={}, service={}, payload_size={}", 
              request.request_id, request.service_name, request.payload.size());
    
    // Step 1: Route to the appropriate handler
    auto it = services_.find(request.service_name);
    
    if (it == services_.end()) {
        // Service not found
        LOG_WARN("RpcServer", "Service not found: '{}' (request_id={})", 
                 request.service_name, request.request_id);
        
        return create_error_response(
            request.request_id,
            ErrorCode::ServiceNotFound,
            "Service '" + request.service_name + "' not found"
        );
    }
    
    // Step 2: Execute the handler
    try {
        LOG_DEBUG("RpcServer", "Executing service handler: '{}' (request_id={})", 
                  request.service_name, request.request_id);
        
        Response response = it->second(request);
        
        if (response.error_code == ErrorCode::Success) {
            LOG_DEBUG("RpcServer", "Service executed successfully: '{}' (request_id={}, response_size={})", 
                      request.service_name, request.request_id, response.payload.size());
        } else {
            LOG_WARN("RpcServer", "Service returned error: '{}' (request_id={}, error_code={}, error_msg={})", 
                     request.service_name, request.request_id, 
                     static_cast<uint32_t>(response.error_code), response.error_message);
        }
        
        return response;
    } catch (const std::exception& e) {
        // Handler threw an exception
        LOG_ERROR("RpcServer", "Service execution exception: '{}' (request_id={}, exception={})", 
                  request.service_name, request.request_id, e.what());
        
        return create_error_response(
            request.request_id,
            ErrorCode::ServiceException,
            std::string("Service execution failed: ") + e.what()
        );
    } catch (...) {
        // Unknown exception
        LOG_ERROR("RpcServer", "Service execution unknown exception: '{}' (request_id={})", 
                  request.service_name, request.request_id);
        
        return create_error_response(
            request.request_id,
            ErrorCode::ServiceException,
            "Service execution failed with unknown exception"
        );
    }
}

size_t RpcServer::service_count() const {
    return services_.size();
}

Response RpcServer::create_error_response(uint32_t request_id,
                                         ErrorCode error_code,
                                         const std::string& error_message) const {
    Response response;
    response.request_id = request_id;
    response.error_code = error_code;
    response.error_message = error_message;
    response.payload.clear();
    return response;
}

void RpcServer::record_request_stats(bool success, double latency_ms) {
    // Update atomic counters
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    if (success) {
        successful_requests_.fetch_add(1, std::memory_order_relaxed);
    } else {
        failed_requests_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Update latency and timestamp tracking (requires lock)
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Record latency
    latencies_.push_back(latency_ms);
    total_latency_ms_ += latency_ms;
    
    // Keep only recent latency samples to avoid unbounded growth
    if (latencies_.size() > MAX_LATENCY_SAMPLES) {
        // Remove oldest sample
        total_latency_ms_ -= latencies_.front();
        latencies_.erase(latencies_.begin());
    }
    
    // Record timestamp for QPS calculation
    auto now = std::chrono::steady_clock::now();
    request_timestamps_.push_back(now);
    
    // Remove timestamps older than the QPS window
    auto cutoff = now - std::chrono::seconds(QPS_WINDOW_SECONDS);
    while (!request_timestamps_.empty() && request_timestamps_.front() < cutoff) {
        request_timestamps_.pop_front();
    }
}

double RpcServer::calculate_qps() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (request_timestamps_.empty()) {
        return 0.0;
    }
    
    if (request_timestamps_.size() == 1) {
        // Only one request, can't calculate meaningful QPS yet
        return 0.0;
    }
    
    // Calculate time span of the window
    auto now = std::chrono::steady_clock::now();
    auto oldest = request_timestamps_.front();
    auto duration_seconds = std::chrono::duration<double>(now - oldest).count();
    
    if (duration_seconds < 0.000001) {
        // Extremely short duration, avoid division by zero
        // Return a very high QPS estimate
        return static_cast<double>(request_timestamps_.size()) * 1000000.0;
    }
    
    // QPS = number of requests / time span
    return static_cast<double>(request_timestamps_.size()) / duration_seconds;
}

ServerStats RpcServer::get_stats() const {
    ServerStats stats;
    
    // Get atomic counters
    stats.total_requests = total_requests_.load(std::memory_order_relaxed);
    stats.successful_requests = successful_requests_.load(std::memory_order_relaxed);
    stats.failed_requests = failed_requests_.load(std::memory_order_relaxed);
    
    // Active connections is always 0 in MVP (no network engine)
    stats.active_connections = 0;
    
    // Calculate latency metrics (requires lock)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        if (!latencies_.empty()) {
            // Calculate average latency
            stats.avg_latency_ms = total_latency_ms_ / latencies_.size();
            
            // Calculate P99 latency
            std::vector<double> sorted_latencies = latencies_;
            std::sort(sorted_latencies.begin(), sorted_latencies.end());
            
            size_t p99_index = static_cast<size_t>(sorted_latencies.size() * 0.99);
            if (p99_index >= sorted_latencies.size()) {
                p99_index = sorted_latencies.size() - 1;
            }
            stats.p99_latency_ms = sorted_latencies[p99_index];
        }
    }
    
    // Calculate QPS
    stats.qps = calculate_qps();
    
    return stats;
}

} // namespace frpc
