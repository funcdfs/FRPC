#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace frpc {

/**
 * @brief 内存池 - 用于协程帧分配
 * 
 * 预分配固定大小的内存块，减少堆分配开销和内存碎片。
 * 使用空闲链表管理可用内存块。
 * 
 * 使用场景：
 * - 协程帧内存分配（Coroutine Scheduler）
 * - 频繁分配和释放固定大小对象的场景
 * 
 * 限制：
 * - 只支持固定大小的内存块分配
 * - 如果请求的大小超过 block_size，allocate 将返回 nullptr
 * - 线程安全，但高并发场景下可能存在锁竞争
 * 
 * @note 线程安全：所有公共方法都是线程安全的
 */
class MemoryPool {
public:
    /**
     * @brief 构造函数
     * @param block_size 每个内存块的大小（字节）
     * @param initial_blocks 初始内存块数量
     * 
     * @example
     * // 创建用于协程帧的内存池（每个块 4KB，初始 100 个块）
     * MemoryPool pool(4096, 100);
     */
    MemoryPool(size_t block_size, size_t initial_blocks);
    
    /**
     * @brief 析构函数 - 释放所有内存块
     */
    ~MemoryPool();
    
    // 禁止拷贝和移动
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;
    
    /**
     * @brief 分配内存
     * @param size 需要的字节数
     * @return 内存指针，如果 size 超过 block_size 或内存池耗尽则返回 nullptr
     * 
     * 优先从空闲链表获取内存块，如果空闲链表为空则扩展内存池。
     * 
     * @note 线程安全
     * 
     * @example
     * void* ptr = pool.allocate(1024);
     * if (ptr) {
     *     // 使用内存...
     *     pool.deallocate(ptr);
     * }
     */
    void* allocate(size_t size);
    
    /**
     * @brief 释放内存
     * @param ptr 内存指针（必须是由此内存池分配的）
     * 
     * 将内存块放回空闲链表，供后续分配使用。
     * 
     * @note 线程安全
     * @warning 传入非此内存池分配的指针会导致未定义行为
     */
    void deallocate(void* ptr);
    
    /**
     * @brief 统计信息结构
     */
    struct Stats {
        size_t total_blocks;      ///< 总内存块数
        size_t free_blocks;       ///< 空闲内存块数
        size_t allocations;       ///< 累计分配次数
        size_t deallocations;     ///< 累计释放次数
    };
    
    /**
     * @brief 获取统计信息
     * @return 内存池统计信息
     * 
     * @note 线程安全
     */
    Stats get_stats() const;
    
    /**
     * @brief 获取内存块大小
     * @return 每个内存块的大小（字节）
     */
    size_t block_size() const { return block_size_; }

private:
    /**
     * @brief 空闲块节点（使用侵入式链表）
     */
    struct Block {
        Block* next;  ///< 指向下一个空闲块
    };
    
    /**
     * @brief 扩展内存池（分配新的内存块）
     * @param num_blocks 要分配的块数
     * 
     * @note 调用者必须持有 mutex_
     */
    void expand(size_t num_blocks);
    
    size_t block_size_;                          ///< 每个内存块的大小
    std::vector<std::unique_ptr<uint8_t[]>> chunks_;  ///< 内存块存储
    Block* free_list_;                           ///< 空闲链表头
    mutable std::mutex mutex_;                   ///< 保护并发访问
    Stats stats_;                                ///< 统计信息
};

}  // namespace frpc
