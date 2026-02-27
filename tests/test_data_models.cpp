/**
 * @file test_data_models.cpp
 * @brief 数据模型单元测试
 * 
 * 测试 Request 和 Response 数据结构的基本功能。
 */

#include <gtest/gtest.h>
#include "frpc/data_models.h"
#include <thread>
#include <vector>
#include <set>

using namespace frpc;

/**
 * @brief 测试 Request 结构体的基本字段
 */
TEST(DataModelsTest, RequestBasicFields) {
    Request request;
    
    // 设置基本字段
    request.request_id = 123;
    request.service_name = "test_service";
    request.payload = {1, 2, 3, 4, 5};
    request.timeout = std::chrono::milliseconds(5000);
    request.metadata["trace_id"] = "abc-123";
    request.metadata["client_version"] = "1.0.0";
    
    // 验证字段值
    EXPECT_EQ(request.request_id, 123u);
    EXPECT_EQ(request.service_name, "test_service");
    EXPECT_EQ(request.payload.size(), 5u);
    EXPECT_EQ(request.payload[0], 1);
    EXPECT_EQ(request.timeout.count(), 5000);
    EXPECT_EQ(request.metadata.size(), 2u);
    EXPECT_EQ(request.metadata["trace_id"], "abc-123");
    EXPECT_EQ(request.metadata["client_version"], "1.0.0");
}

/**
 * @brief 测试 Response 结构体的基本字段
 */
TEST(DataModelsTest, ResponseBasicFields) {
    Response response;
    
    // 设置基本字段
    response.request_id = 456;
    response.error_code = ErrorCode::Success;
    response.error_message = "";
    response.payload = {10, 20, 30};
    response.metadata["server_id"] = "server-1";
    response.metadata["process_time_ms"] = "42";
    
    // 验证字段值
    EXPECT_EQ(response.request_id, 456u);
    EXPECT_EQ(response.error_code, ErrorCode::Success);
    EXPECT_TRUE(response.error_message.empty());
    EXPECT_EQ(response.payload.size(), 3u);
    EXPECT_EQ(response.payload[0], 10);
    EXPECT_EQ(response.metadata.size(), 2u);
    EXPECT_EQ(response.metadata["server_id"], "server-1");
}

/**
 * @brief 测试 Response 错误情况
 */
TEST(DataModelsTest, ResponseWithError) {
    Response response;
    
    // 设置错误响应
    response.request_id = 789;
    response.error_code = ErrorCode::ServiceNotFound;
    response.error_message = "Service 'unknown_service' not found";
    response.payload = {};  // 错误响应通常没有 payload
    
    // 验证错误信息
    EXPECT_EQ(response.request_id, 789u);
    EXPECT_EQ(response.error_code, ErrorCode::ServiceNotFound);
    EXPECT_FALSE(response.error_message.empty());
    EXPECT_EQ(response.error_message, "Service 'unknown_service' not found");
    EXPECT_TRUE(response.payload.empty());
}

/**
 * @brief 测试 Request::generate_id() 生成唯一 ID
 */
TEST(DataModelsTest, GenerateUniqueRequestId) {
    // 生成多个 ID
    std::set<uint32_t> ids;
    const int count = 1000;
    
    for (int i = 0; i < count; ++i) {
        uint32_t id = Request::generate_id();
        
        // 验证 ID 非零
        EXPECT_NE(id, 0u);
        
        // 验证 ID 唯一
        EXPECT_TRUE(ids.insert(id).second) << "Duplicate ID: " << id;
    }
    
    // 验证生成了正确数量的唯一 ID
    EXPECT_EQ(ids.size(), count);
}

/**
 * @brief 测试 Request::generate_id() 的递增性
 */
TEST(DataModelsTest, GenerateIdIsIncremental) {
    // 生成一系列 ID
    std::vector<uint32_t> ids;
    const int count = 100;
    
    for (int i = 0; i < count; ++i) {
        ids.push_back(Request::generate_id());
    }
    
    // 验证 ID 是递增的
    for (size_t i = 1; i < ids.size(); ++i) {
        EXPECT_GT(ids[i], ids[i-1]) << "ID not incremental at index " << i;
    }
}

/**
 * @brief 测试 Request::generate_id() 的线程安全性
 * 
 * 在多线程环境下并发生成 ID，验证不会产生重复的 ID。
 */
TEST(DataModelsTest, GenerateIdThreadSafety) {
    const int num_threads = 10;
    const int ids_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<uint32_t>> thread_ids(num_threads);
    
    // 启动多个线程并发生成 ID
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&thread_ids, t, ids_per_thread]() {
            for (int i = 0; i < ids_per_thread; ++i) {
                thread_ids[t].push_back(Request::generate_id());
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 收集所有生成的 ID
    std::set<uint32_t> all_ids;
    for (const auto& ids : thread_ids) {
        for (uint32_t id : ids) {
            // 验证 ID 非零
            EXPECT_NE(id, 0u);
            
            // 验证 ID 唯一（没有重复）
            EXPECT_TRUE(all_ids.insert(id).second) << "Duplicate ID: " << id;
        }
    }
    
    // 验证生成了正确数量的唯一 ID
    EXPECT_EQ(all_ids.size(), num_threads * ids_per_thread);
}

/**
 * @brief 测试 Request 的空 payload
 */
TEST(DataModelsTest, RequestEmptyPayload) {
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "ping_service";
    request.payload = {};  // 空 payload
    request.timeout = std::chrono::milliseconds(1000);
    
    EXPECT_TRUE(request.payload.empty());
    EXPECT_EQ(request.payload.size(), 0u);
}

/**
 * @brief 测试 Request 的大 payload
 */
TEST(DataModelsTest, RequestLargePayload) {
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "data_service";
    
    // 创建一个大的 payload（1MB）
    const size_t payload_size = 1024 * 1024;
    request.payload.resize(payload_size);
    for (size_t i = 0; i < payload_size; ++i) {
        request.payload[i] = static_cast<uint8_t>(i % 256);
    }
    
    EXPECT_EQ(request.payload.size(), payload_size);
    EXPECT_EQ(request.payload[0], 0);
    EXPECT_EQ(request.payload[255], 255);
}

/**
 * @brief 测试 Request 的空 metadata
 */
TEST(DataModelsTest, RequestEmptyMetadata) {
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "simple_service";
    request.payload = {1, 2, 3};
    request.timeout = std::chrono::milliseconds(3000);
    // metadata 默认为空
    
    EXPECT_TRUE(request.metadata.empty());
    EXPECT_EQ(request.metadata.size(), 0u);
}

/**
 * @brief 测试 Response 的空 metadata
 */
TEST(DataModelsTest, ResponseEmptyMetadata) {
    Response response;
    response.request_id = 100;
    response.error_code = ErrorCode::Success;
    response.payload = {5, 6, 7};
    // metadata 默认为空
    
    EXPECT_TRUE(response.metadata.empty());
    EXPECT_EQ(response.metadata.size(), 0u);
}

/**
 * @brief 测试 Request 和 Response 的 ID 匹配
 */
TEST(DataModelsTest, RequestResponseIdMatching) {
    // 创建请求
    Request request;
    request.request_id = Request::generate_id();
    request.service_name = "echo_service";
    request.payload = {1, 2, 3};
    
    // 创建对应的响应
    Response response;
    response.request_id = request.request_id;  // 匹配请求 ID
    response.error_code = ErrorCode::Success;
    response.payload = request.payload;  // 回显数据
    
    // 验证 ID 匹配
    EXPECT_EQ(response.request_id, request.request_id);
}
