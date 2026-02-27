#include "frpc/memory_pool.h"
#include <algorithm>
#include <stdexcept>

namespace frpc {

MemoryPool::MemoryPool(size_t block_size, size_t initial_blocks)
    : block_size_(block_size), free_list_(nullptr), stats_{} {
    if (block_size < sizeof(Block)) {
        throw std::invalid_argument("block_size must be at least " + 
                                    std::to_string(sizeof(Block)) + " bytes");
    }
    if (initial_blocks == 0) {
        throw std::invalid_argument("initial_blocks must be greater than 0");
    }
    
    // 预分配初始内存块
    expand(initial_blocks);
}

MemoryPool::~MemoryPool() {
    // unique_ptr 会自动释放所有内存
}

void* MemoryPool::allocate(size_t size) {
    if (size > block_size_) {
        return nullptr;  // 请求的大小超过块大小
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果空闲链表为空，扩展内存池
    if (free_list_ == nullptr) {
        // 扩展时分配当前总块数的 50%，至少 10 个块
        size_t expand_count = std::max(stats_.total_blocks / 2, size_t(10));
        expand(expand_count);
    }
    
    // 从空闲链表头部取出一个块
    Block* block = free_list_;
    free_list_ = block->next;
    
    // 更新统计信息
    stats_.allocations++;
    stats_.free_blocks--;
    
    return static_cast<void*>(block);
}

void MemoryPool::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 将块放回空闲链表头部
    Block* block = static_cast<Block*>(ptr);
    block->next = free_list_;
    free_list_ = block;
    
    // 更新统计信息
    stats_.deallocations++;
    stats_.free_blocks++;
}

MemoryPool::Stats MemoryPool::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void MemoryPool::expand(size_t num_blocks) {
    // 分配一大块连续内存
    auto chunk = std::make_unique<uint8_t[]>(block_size_ * num_blocks);
    uint8_t* chunk_ptr = chunk.get();
    
    // 将这块内存分割成多个块，并链接到空闲链表
    for (size_t i = 0; i < num_blocks; ++i) {
        Block* block = reinterpret_cast<Block*>(chunk_ptr + i * block_size_);
        block->next = free_list_;
        free_list_ = block;
    }
    
    // 保存这块内存的所有权
    chunks_.push_back(std::move(chunk));
    
    // 更新统计信息
    stats_.total_blocks += num_blocks;
    stats_.free_blocks += num_blocks;
}

}  // namespace frpc
