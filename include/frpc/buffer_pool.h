#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace frpc {

/**
 * @brief 缓冲区池 - 用于网络 I/O 缓冲区
 * 
 * 预分配固定大小的缓冲区，避免频繁的内存分配和释放。
 * 使用 unique_ptr 管理缓冲区生命周期，确保异常安全。
 * 
 * 使用场景：
 * - 网络 I/O 缓冲区（Network Engine）
 * - 临时数据缓冲区
 * - 需要频繁分配和释放固定大小缓冲区的场景
 * 
 * 限制：
 * - 只支持固定大小的缓冲区
 * - 缓冲区大小在构造时确定，不可更改
 * - 线程安全，但高并发场景下可能存在锁竞争
 * 
 * @note 线程安全：所有公共方法都是线程安全的
 * 
 * @example
 * BufferPool pool(4096, 100);  // 4KB 缓冲区，初始 100 个
 * auto buffer = pool.acquire();
 * // 使用缓冲区...
 * pool.release(std::move(buffer));  // 归还缓冲区
 */
class BufferPool {
public:
    /**
     * @brief 构造函数
     * @param buffer_size 每个缓冲区的大小（字节）
     * @param initial_buffers 初始缓冲区数量
     * 
     * @throws std::invalid_argument 如果 buffer_size 为 0 或 initial_buffers 为 0
     * 
     * @example
     * // 创建用于网络 I/O 的缓冲区池（每个缓冲区 8KB，初始 50 个）
     * BufferPool pool(8192, 50);
     */
    BufferPool(size_t buffer_size, size_t initial_buffers);
    
    /**
     * @brief 析构函数 - 释放所有缓冲区
     */
    ~BufferPool() = default;
    
    // 禁止拷贝和移动
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(BufferPool&&) = delete;
    
    /**
     * @brief 获取缓冲区
     * @return 缓冲区的 unique_ptr，如果缓冲区池为空则创建新缓冲区
     * 
     * 优先从空闲缓冲区列表获取，如果列表为空则分配新缓冲区。
     * 使用 unique_ptr 确保异常安全和自动资源管理。
     * 
     * @note 线程安全
     * @note 返回的缓冲区大小为 buffer_size()
     * 
     * @example
     * auto buffer = pool.acquire();
     * // 使用缓冲区...
     * std::memcpy(buffer.get(), data, size);
     * // unique_ptr 会自动管理生命周期
     * pool.release(std::move(buffer));
     */
    std::unique_ptr<uint8_t[]> acquire();
    
    /**
     * @brief 归还缓冲区
     * @param buffer 缓冲区指针（必须是由此缓冲区池分配的）
     * 
     * 将缓冲区放回空闲列表，供后续 acquire 调用使用。
     * 如果传入 nullptr，则忽略。
     * 
     * @note 线程安全
     * @warning 传入非此缓冲区池分配的指针会导致未定义行为
     * 
     * @example
     * auto buffer = pool.acquire();
     * // 使用完毕后归还
     * pool.release(std::move(buffer));
     */
    void release(std::unique_ptr<uint8_t[]> buffer);
    
    /**
     * @brief 获取缓冲区大小
     * @return 每个缓冲区的大小（字节）
     */
    size_t buffer_size() const { return buffer_size_; }
    
    /**
     * @brief 统计信息结构
     */
    struct Stats {
        size_t total_buffers;     ///< 总缓冲区数（包括空闲和使用中的）
        size_t free_buffers;      ///< 空闲缓冲区数
        size_t acquisitions;      ///< 累计获取次数
        size_t releases;          ///< 累计归还次数
    };
    
    /**
     * @brief 获取统计信息
     * @return 缓冲区池统计信息
     * 
     * @note 线程安全
     */
    Stats get_stats() const;

private:
    size_t buffer_size_;                                    ///< 每个缓冲区的大小
    std::vector<std::unique_ptr<uint8_t[]>> free_buffers_;  ///< 空闲缓冲区列表
    mutable std::mutex mutex_;                              ///< 保护并发访问
    Stats stats_;                                           ///< 统计信息
};

}  // namespace frpc
