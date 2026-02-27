#include "frpc/buffer_pool.h"
#include <stdexcept>

namespace frpc {

BufferPool::BufferPool(size_t buffer_size, size_t initial_buffers)
    : buffer_size_(buffer_size), stats_{} {
    if (buffer_size == 0) {
        throw std::invalid_argument("buffer_size must be greater than 0");
    }
    if (initial_buffers == 0) {
        throw std::invalid_argument("initial_buffers must be greater than 0");
    }
    
    // 预分配初始缓冲区
    free_buffers_.reserve(initial_buffers);
    for (size_t i = 0; i < initial_buffers; ++i) {
        free_buffers_.push_back(std::make_unique<uint8_t[]>(buffer_size_));
    }
    
    // 初始化统计信息
    stats_.total_buffers = initial_buffers;
    stats_.free_buffers = initial_buffers;
    stats_.acquisitions = 0;
    stats_.releases = 0;
}

std::unique_ptr<uint8_t[]> BufferPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::unique_ptr<uint8_t[]> buffer;
    
    if (!free_buffers_.empty()) {
        // 从空闲列表获取缓冲区
        buffer = std::move(free_buffers_.back());
        free_buffers_.pop_back();
        stats_.free_buffers--;
    } else {
        // 空闲列表为空，分配新缓冲区
        buffer = std::make_unique<uint8_t[]>(buffer_size_);
        stats_.total_buffers++;
    }
    
    stats_.acquisitions++;
    return buffer;
}

void BufferPool::release(std::unique_ptr<uint8_t[]> buffer) {
    if (!buffer) {
        return;  // 忽略 nullptr
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 将缓冲区放回空闲列表
    free_buffers_.push_back(std::move(buffer));
    stats_.free_buffers++;
    stats_.releases++;
}

BufferPool::Stats BufferPool::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

}  // namespace frpc
