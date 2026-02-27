#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "frpc/memory_pool.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace frpc;

// ============================================================================
// Property-Based Tests
// ============================================================================

/**
 * Feature: frpc-framework, Property 32 变体: 内存池线程安全
 * 
 * **Validates: Requirements 13.4**
 * 
 * 测试多线程并发分配和释放不会导致数据竞争，
 * 且分配和释放次数的一致性得到保证。
 */
RC_GTEST_PROP(MemoryPoolPropertyTest, ThreadSafety, 
              (int num_threads, int ops_per_thread)) {
    // 前置条件：线程数在合理范围内
    RC_PRE(num_threads >= 2 && num_threads <= 10);
    RC_PRE(ops_per_thread >= 10 && ops_per_thread <= 100);
    
    // 创建内存池（块大小 64 字节，初始 10 个块）
    MemoryPool pool(64, 10);
    
    std::atomic<int> successful_allocations{0};
    std::atomic<int> successful_deallocations{0};
    std::vector<std::thread> threads;
    
    // 多线程并发分配和释放
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, ops_per_thread]() {
            std::vector<void*> allocated_ptrs;
            
            for (int j = 0; j < ops_per_thread; ++j) {
                // 分配内存
                void* ptr = pool.allocate(64);
                if (ptr != nullptr) {
                    successful_allocations++;
                    allocated_ptrs.push_back(ptr);
                }
            }
            
            // 释放所有分配的内存
            for (void* ptr : allocated_ptrs) {
                pool.deallocate(ptr);
                successful_deallocations++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证：成功分配的次数应该等于成功释放的次数
    RC_ASSERT(successful_allocations.load() == successful_deallocations.load());
    
    // 验证：内存池状态一致性
    auto stats = pool.get_stats();
    RC_ASSERT(stats.allocations == successful_allocations.load());
    RC_ASSERT(stats.deallocations == successful_deallocations.load());
    
    // 验证：所有内存都已归还（空闲块数应该等于总块数）
    RC_ASSERT(stats.free_blocks == stats.total_blocks);
}

/**
 * Property: 内存池分配和释放的往返属性
 * 
 * 对于任何有效的分配，释放后应该能够再次分配相同大小的内存。
 */
RC_GTEST_PROP(MemoryPoolPropertyTest, AllocationRoundTrip, (size_t alloc_size)) {
    // 前置条件：分配大小在合理范围内
    RC_PRE(alloc_size > 0 && alloc_size <= 1024);
    
    MemoryPool pool(1024, 10);
    
    // 第一次分配
    void* ptr1 = pool.allocate(alloc_size);
    RC_ASSERT(ptr1 != nullptr);
    
    // 释放
    pool.deallocate(ptr1);
    
    // 第二次分配（应该成功，可能返回相同的指针）
    void* ptr2 = pool.allocate(alloc_size);
    RC_ASSERT(ptr2 != nullptr);
    
    // 清理
    pool.deallocate(ptr2);
}

/**
 * Property: 超过块大小的分配应该返回 nullptr
 */
RC_GTEST_PROP(MemoryPoolPropertyTest, OversizedAllocationFails, (size_t block_size)) {
    RC_PRE(block_size >= 16 && block_size <= 1024);
    
    MemoryPool pool(block_size, 10);
    
    // 尝试分配超过块大小的内存
    void* ptr = pool.allocate(block_size + 1);
    RC_ASSERT(ptr == nullptr);
}

// ============================================================================
// Unit Tests
// ============================================================================

/**
 * 基本功能测试：分配和释放
 */
TEST(MemoryPoolTest, BasicAllocationAndDeallocation) {
    MemoryPool pool(64, 10);
    
    // 分配内存
    void* ptr = pool.allocate(64);
    ASSERT_NE(ptr, nullptr);
    
    // 检查统计信息
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.allocations, 1);
    EXPECT_EQ(stats.total_blocks, 10);
    EXPECT_EQ(stats.free_blocks, 9);
    
    // 释放内存
    pool.deallocate(ptr);
    
    // 检查统计信息
    stats = pool.get_stats();
    EXPECT_EQ(stats.deallocations, 1);
    EXPECT_EQ(stats.free_blocks, 10);
}

/**
 * 测试多次分配
 */
TEST(MemoryPoolTest, MultipleAllocations) {
    MemoryPool pool(64, 5);
    
    std::vector<void*> ptrs;
    
    // 分配 5 个块
    for (int i = 0; i < 5; ++i) {
        void* ptr = pool.allocate(64);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }
    
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.allocations, 5);
    EXPECT_EQ(stats.free_blocks, 0);
    
    // 释放所有块
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.deallocations, 5);
    EXPECT_EQ(stats.free_blocks, 5);
}

/**
 * 测试内存池自动扩展
 */
TEST(MemoryPoolTest, AutoExpansion) {
    MemoryPool pool(64, 2);
    
    std::vector<void*> ptrs;
    
    // 分配超过初始块数的内存
    for (int i = 0; i < 10; ++i) {
        void* ptr = pool.allocate(64);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }
    
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.allocations, 10);
    EXPECT_GT(stats.total_blocks, 2);  // 应该已经扩展
    
    // 清理
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }
}

/**
 * 测试超大分配失败
 */
TEST(MemoryPoolTest, OversizedAllocationFails) {
    MemoryPool pool(64, 10);
    
    // 尝试分配超过块大小的内存
    void* ptr = pool.allocate(128);
    EXPECT_EQ(ptr, nullptr);
    
    // 统计信息不应该改变
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.allocations, 0);
}

/**
 * 测试释放 nullptr
 */
TEST(MemoryPoolTest, DeallocateNullptr) {
    MemoryPool pool(64, 10);
    
    // 释放 nullptr 应该安全
    EXPECT_NO_THROW(pool.deallocate(nullptr));
    
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.deallocations, 0);
}

/**
 * 测试无效参数
 */
TEST(MemoryPoolTest, InvalidParameters) {
    // 块大小太小
    EXPECT_THROW(MemoryPool(4, 10), std::invalid_argument);
    
    // 初始块数为 0
    EXPECT_THROW(MemoryPool(64, 0), std::invalid_argument);
}

/**
 * 测试内存复用
 */
TEST(MemoryPoolTest, MemoryReuse) {
    MemoryPool pool(64, 1);
    
    // 第一次分配
    void* ptr1 = pool.allocate(64);
    ASSERT_NE(ptr1, nullptr);
    
    // 释放
    pool.deallocate(ptr1);
    
    // 第二次分配应该返回相同的指针（内存复用）
    void* ptr2 = pool.allocate(64);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr1, ptr2);
    
    // 清理
    pool.deallocate(ptr2);
}

/**
 * 测试统计信息准确性
 */
TEST(MemoryPoolTest, StatisticsAccuracy) {
    MemoryPool pool(64, 10);
    
    // 初始状态
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_blocks, 10);
    EXPECT_EQ(stats.free_blocks, 10);
    EXPECT_EQ(stats.allocations, 0);
    EXPECT_EQ(stats.deallocations, 0);
    
    // 分配 3 个块
    void* ptr1 = pool.allocate(64);
    void* ptr2 = pool.allocate(64);
    void* ptr3 = pool.allocate(64);
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.allocations, 3);
    EXPECT_EQ(stats.free_blocks, 7);
    
    // 释放 2 个块
    pool.deallocate(ptr1);
    pool.deallocate(ptr2);
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.deallocations, 2);
    EXPECT_EQ(stats.free_blocks, 9);
    
    // 清理
    pool.deallocate(ptr3);
}
