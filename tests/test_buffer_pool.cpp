#include <gtest/gtest.h>
#include "frpc/buffer_pool.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

using namespace frpc;

// ============================================================================
// Unit Tests
// ============================================================================

/**
 * 基本功能测试：获取和归还缓冲区
 * 
 * **Validates: Requirements 1.5**
 */
TEST(BufferPoolTest, BasicAcquireAndRelease) {
    BufferPool pool(1024, 10);
    
    // 获取缓冲区
    auto buffer = pool.acquire();
    ASSERT_NE(buffer, nullptr);
    
    // 检查统计信息
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.acquisitions, 1);
    EXPECT_EQ(stats.total_buffers, 10);
    EXPECT_EQ(stats.free_buffers, 9);
    
    // 归还缓冲区
    pool.release(std::move(buffer));
    
    // 检查统计信息
    stats = pool.get_stats();
    EXPECT_EQ(stats.releases, 1);
    EXPECT_EQ(stats.free_buffers, 10);
}

/**
 * 测试缓冲区大小正确性
 * 
 * **Validates: Requirements 1.5**
 */
TEST(BufferPoolTest, BufferSizeCorrectness) {
    size_t buffer_size = 4096;
    BufferPool pool(buffer_size, 5);
    
    // 验证缓冲区大小
    EXPECT_EQ(pool.buffer_size(), buffer_size);
    
    // 获取缓冲区并验证可以写入指定大小的数据
    auto buffer = pool.acquire();
    ASSERT_NE(buffer, nullptr);
    
    // 写入数据（不应该崩溃）
    std::memset(buffer.get(), 0xAB, buffer_size);
    
    // 验证数据
    for (size_t i = 0; i < buffer_size; ++i) {
        EXPECT_EQ(buffer[i], 0xAB);
    }
    
    pool.release(std::move(buffer));
}

/**
 * 测试多次获取和归还
 */
TEST(BufferPoolTest, MultipleAcquireAndRelease) {
    BufferPool pool(512, 5);
    
    std::vector<std::unique_ptr<uint8_t[]>> buffers;
    
    // 获取 5 个缓冲区
    for (int i = 0; i < 5; ++i) {
        auto buffer = pool.acquire();
        ASSERT_NE(buffer, nullptr);
        buffers.push_back(std::move(buffer));
    }
    
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.acquisitions, 5);
    EXPECT_EQ(stats.free_buffers, 0);
    
    // 归还所有缓冲区
    for (auto& buffer : buffers) {
        pool.release(std::move(buffer));
    }
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.releases, 5);
    EXPECT_EQ(stats.free_buffers, 5);
}

/**
 * 测试缓冲区池自动扩展
 */
TEST(BufferPoolTest, AutoExpansion) {
    BufferPool pool(256, 2);
    
    std::vector<std::unique_ptr<uint8_t[]>> buffers;
    
    // 获取超过初始数量的缓冲区
    for (int i = 0; i < 10; ++i) {
        auto buffer = pool.acquire();
        ASSERT_NE(buffer, nullptr);
        buffers.push_back(std::move(buffer));
    }
    
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.acquisitions, 10);
    EXPECT_GT(stats.total_buffers, 2);  // 应该已经扩展
    EXPECT_EQ(stats.free_buffers, 0);
    
    // 清理
    for (auto& buffer : buffers) {
        pool.release(std::move(buffer));
    }
}

/**
 * 测试归还 nullptr
 */
TEST(BufferPoolTest, ReleaseNullptr) {
    BufferPool pool(1024, 10);
    
    // 归还 nullptr 应该安全
    EXPECT_NO_THROW(pool.release(nullptr));
    
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.releases, 0);
}

/**
 * 测试无效参数
 */
TEST(BufferPoolTest, InvalidParameters) {
    // 缓冲区大小为 0
    EXPECT_THROW(BufferPool(0, 10), std::invalid_argument);
    
    // 初始缓冲区数为 0
    EXPECT_THROW(BufferPool(1024, 0), std::invalid_argument);
}

/**
 * 测试缓冲区复用
 */
TEST(BufferPoolTest, BufferReuse) {
    BufferPool pool(1024, 1);
    
    // 第一次获取
    auto buffer1 = pool.acquire();
    ASSERT_NE(buffer1, nullptr);
    uint8_t* ptr1 = buffer1.get();
    
    // 归还
    pool.release(std::move(buffer1));
    
    // 第二次获取应该返回相同的缓冲区（内存复用）
    auto buffer2 = pool.acquire();
    ASSERT_NE(buffer2, nullptr);
    uint8_t* ptr2 = buffer2.get();
    
    EXPECT_EQ(ptr1, ptr2);
    
    // 清理
    pool.release(std::move(buffer2));
}

/**
 * 测试统计信息准确性
 */
TEST(BufferPoolTest, StatisticsAccuracy) {
    BufferPool pool(2048, 10);
    
    // 初始状态
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_buffers, 10);
    EXPECT_EQ(stats.free_buffers, 10);
    EXPECT_EQ(stats.acquisitions, 0);
    EXPECT_EQ(stats.releases, 0);
    
    // 获取 3 个缓冲区
    auto buffer1 = pool.acquire();
    auto buffer2 = pool.acquire();
    auto buffer3 = pool.acquire();
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.acquisitions, 3);
    EXPECT_EQ(stats.free_buffers, 7);
    
    // 归还 2 个缓冲区
    pool.release(std::move(buffer1));
    pool.release(std::move(buffer2));
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.releases, 2);
    EXPECT_EQ(stats.free_buffers, 9);
    
    // 清理
    pool.release(std::move(buffer3));
}

/**
 * 测试 unique_ptr 自动管理
 */
TEST(BufferPoolTest, UniquePtrAutoManagement) {
    BufferPool pool(1024, 5);
    
    {
        // 在作用域内获取缓冲区
        auto buffer = pool.acquire();
        ASSERT_NE(buffer, nullptr);
        
        auto stats = pool.get_stats();
        EXPECT_EQ(stats.acquisitions, 1);
        EXPECT_EQ(stats.free_buffers, 4);
        
        // unique_ptr 会在作用域结束时自动释放
        // 但不会自动归还到缓冲区池（需要显式调用 release）
    }
    
    // 缓冲区已被 unique_ptr 释放，但未归还到池中
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.releases, 0);  // 未调用 release
}

/**
 * 测试缓冲区数据独立性
 */
TEST(BufferPoolTest, BufferDataIndependence) {
    BufferPool pool(1024, 2);
    
    // 获取两个缓冲区
    auto buffer1 = pool.acquire();
    auto buffer2 = pool.acquire();
    
    // 写入不同的数据
    std::memset(buffer1.get(), 0xAA, 1024);
    std::memset(buffer2.get(), 0xBB, 1024);
    
    // 验证数据独立
    EXPECT_EQ(buffer1[0], 0xAA);
    EXPECT_EQ(buffer2[0], 0xBB);
    
    // 清理
    pool.release(std::move(buffer1));
    pool.release(std::move(buffer2));
}

/**
 * 测试线程安全（基本并发测试）
 */
TEST(BufferPoolTest, BasicThreadSafety) {
    BufferPool pool(1024, 10);
    
    std::atomic<int> successful_acquisitions{0};
    std::atomic<int> successful_releases{0};
    std::vector<std::thread> threads;
    
    // 多线程并发获取和归还
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            std::vector<std::unique_ptr<uint8_t[]>> buffers;
            
            // 获取缓冲区
            for (int j = 0; j < 10; ++j) {
                auto buffer = pool.acquire();
                if (buffer) {
                    successful_acquisitions++;
                    buffers.push_back(std::move(buffer));
                }
            }
            
            // 归还缓冲区
            for (auto& buffer : buffers) {
                pool.release(std::move(buffer));
                successful_releases++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证：成功获取的次数应该等于成功归还的次数
    EXPECT_EQ(successful_acquisitions.load(), successful_releases.load());
    EXPECT_EQ(successful_acquisitions.load(), 50);  // 5 threads * 10 ops
    
    // 验证统计信息
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.acquisitions, 50);
    EXPECT_EQ(stats.releases, 50);
}

/**
 * 测试大缓冲区
 */
TEST(BufferPoolTest, LargeBuffers) {
    size_t large_size = 1024 * 1024;  // 1 MB
    BufferPool pool(large_size, 2);
    
    auto buffer = pool.acquire();
    ASSERT_NE(buffer, nullptr);
    
    // 验证可以写入大量数据
    std::memset(buffer.get(), 0xFF, large_size);
    
    // 验证数据
    EXPECT_EQ(buffer[0], 0xFF);
    EXPECT_EQ(buffer[large_size - 1], 0xFF);
    
    pool.release(std::move(buffer));
}

/**
 * 测试小缓冲区
 */
TEST(BufferPoolTest, SmallBuffers) {
    size_t small_size = 64;
    BufferPool pool(small_size, 20);
    
    auto buffer = pool.acquire();
    ASSERT_NE(buffer, nullptr);
    
    // 验证可以写入数据
    std::memset(buffer.get(), 0x12, small_size);
    
    // 验证数据
    for (size_t i = 0; i < small_size; ++i) {
        EXPECT_EQ(buffer[i], 0x12);
    }
    
    pool.release(std::move(buffer));
}
