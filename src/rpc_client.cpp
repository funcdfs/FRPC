/**
 * @file rpc_client.cpp
 * @brief FRPC 框架 RPC 客户端实现
 */

#include "frpc/rpc_client.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace frpc {

RpcClient::RpcClient(TransportFunction transport)
    : transport_(std::move(transport))
    , serializer_(std::make_unique<BinarySerializer>())
    , last_request_id_(0)
    , max_retries_(3)  // 默认重试 3 次
    , stats_()
    , total_latency_ms_(0.0) {
}

RpcClient::~RpcClient() = default;

std::optional<Response> RpcClient::call(
    const std::string& service_name,
    const ByteBuffer& payload,
    Duration timeout
) {
    // 创建空元数据
    std::unordered_map<std::string, std::string> empty_metadata;
    return call_with_metadata(service_name, payload, empty_metadata, timeout);
}

std::optional<Response> RpcClient::call_with_metadata(
    const std::string& service_name,
    const ByteBuffer& payload,
    const std::unordered_map<std::string, std::string>& metadata,
    Duration timeout
) {
    // 创建请求对象
    Request request = create_request(service_name, payload, metadata, timeout);
    
    // 执行带重试的调用
    return do_call_with_retry(request);
}

uint32_t RpcClient::last_request_id() const {
    return last_request_id_;
}

void RpcClient::set_max_retries(size_t max_retries) {
    max_retries_ = max_retries;
}

size_t RpcClient::get_max_retries() const {
    return max_retries_;
}

ClientStats RpcClient::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void RpcClient::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ClientStats{};
    total_latency_ms_ = 0.0;
}

Request RpcClient::create_request(
    const std::string& service_name,
    const ByteBuffer& payload,
    const std::unordered_map<std::string, std::string>& metadata,
    Duration timeout
) {
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = service_name;
    request.payload = payload;
    request.timeout = timeout;
    request.metadata = metadata;
    
    // 记录请求 ID
    last_request_id_ = request.request_id;
    
    return request;
}

std::optional<Response> RpcClient::do_call(const Request& request) {
    try {
        // 步骤 1: 序列化请求 (需求 5.2)
        ByteBuffer request_bytes = serializer_->serialize(request);
        
        // 步骤 2: 通过网络发送请求 (需求 5.3)
        std::optional<ByteBuffer> response_bytes = transport_(request_bytes);
        
        // 步骤 3: 检查网络传输是否成功 (需求 5.6)
        if (!response_bytes) {
            // 网络传输失败 (需求 5.6, 12.1)
            std::cerr << "[ERROR] Network transmission failed for request_id=" 
                      << request.request_id << ", service=" << request.service_name << std::endl;
            return std::nullopt;
        }
        
        // 步骤 4: 反序列化响应 (需求 5.4)
        Response response = serializer_->deserialize_response(*response_bytes);
        
        // 步骤 5: 返回结果给调用者 (需求 5.5)
        return response;
        
    } catch (const SerializationException& e) {
        // 序列化失败
        std::cerr << "[ERROR] Serialization error for request_id=" << request.request_id 
                  << ": " << e.what() << std::endl;
        return std::nullopt;
        
    } catch (const DeserializationException& e) {
        // 反序列化失败 (需求 5.7, 12.1, 12.3)
        std::cerr << "[ERROR] Deserialization error for request_id=" << request.request_id 
                  << ": " << e.what() << std::endl;
        return std::nullopt;
        
    } catch (const std::exception& e) {
        // 其他异常 (需求 12.3)
        std::cerr << "[ERROR] Unexpected error for request_id=" << request.request_id 
                  << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<Response> RpcClient::do_call_with_retry(const Request& request) {
    // 实现自动重试机制 (需求 5.6, 12.1, 12.3)
    size_t attempt = 0;
    
    // 记录开始时间
    auto start_time = std::chrono::steady_clock::now();
    
    while (attempt <= max_retries_) {
        // 尝试调用
        auto response = do_call(request);
        
        // 如果成功，记录统计并返回
        if (response) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            
            // 检查响应是否包含错误
            bool success = (response->error_code == ErrorCode::Success);
            record_call_stats(success, latency);
            
            if (attempt > 0) {
                std::cerr << "[INFO] Request succeeded after " << attempt << " retries, request_id=" 
                          << request.request_id << std::endl;
            }
            return response;
        }
        
        // 如果失败且还有重试机会
        if (attempt < max_retries_) {
            std::cerr << "[WARN] Retry " << (attempt + 1) << "/" << max_retries_ 
                      << " for request_id=" << request.request_id 
                      << ", service=" << request.service_name << std::endl;
            attempt++;
            // 简单的退避策略：等待一小段时间再重试
            // 在完整实现中，这里会使用协程的 sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * attempt));
        } else {
            // 所有重试都失败
            std::cerr << "[ERROR] All retries exhausted for request_id=" << request.request_id 
                      << ", service=" << request.service_name << std::endl;
            break;
        }
    }
    
    // 所有尝试都失败，记录失败统计
    auto end_time = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    record_call_stats(false, latency);
    
    // 返回空
    return std::nullopt;
}

void RpcClient::record_call_stats(bool success, double latency_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_calls++;
    
    if (success) {
        stats_.successful_calls++;
    } else {
        stats_.failed_calls++;
    }
    
    // 更新平均延迟
    total_latency_ms_ += latency_ms;
    stats_.avg_latency_ms = total_latency_ms_ / stats_.total_calls;
}

} // namespace frpc
