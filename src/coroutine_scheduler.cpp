#include "frpc/coroutine_scheduler.h"
#include <stdexcept>

namespace frpc {

CoroutineScheduler::~CoroutineScheduler() {
    // 销毁所有未销毁的协程
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [addr, info] : coroutines_) {
        if (info.handle && !info.handle.done()) {
            info.handle.destroy();
        }
    }
    
    coroutines_.clear();
}

void CoroutineScheduler::suspend(std::coroutine_handle<> handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = coroutines_.find(handle.address());
    if (it == coroutines_.end()) {
        throw std::runtime_error("Coroutine not found in scheduler");
    }
    
    // 更新状态为 Suspended
    it->second.state = CoroutineState::Suspended;
}

void CoroutineScheduler::resume(std::coroutine_handle<> handle) {
    if (!handle) {
        throw std::runtime_error("Invalid coroutine handle");
    }
    
    // 检查协程是否已完成
    if (handle.done()) {
        // 更新状态为 Completed
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = coroutines_.find(handle.address());
        if (it != coroutines_.end()) {
            it->second.state = CoroutineState::Completed;
        }
        return;
    }
    
    // 更新状态为 Running
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = coroutines_.find(handle.address());
        if (it == coroutines_.end()) {
            throw std::runtime_error("Coroutine not found in scheduler");
        }
        
        // 只有 Created 或 Suspended 状态的协程可以恢复
        if (it->second.state != CoroutineState::Created && 
            it->second.state != CoroutineState::Suspended) {
            return;  // 已完成或失败的协程不能恢复
        }
        
        it->second.state = CoroutineState::Running;
    }
    
    // 恢复协程执行
    try {
        handle.resume();
        
        // 检查协程是否完成
        if (handle.done()) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = coroutines_.find(handle.address());
            if (it != coroutines_.end()) {
                it->second.state = CoroutineState::Completed;
            }
        } else {
            // 协程挂起
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = coroutines_.find(handle.address());
            if (it != coroutines_.end()) {
                it->second.state = CoroutineState::Suspended;
            }
        }
    } catch (...) {
        // 捕获异常，标记协程为 Failed
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = coroutines_.find(handle.address());
        if (it != coroutines_.end()) {
            it->second.state = CoroutineState::Failed;
        }
        throw;  // 重新抛出异常
    }
}

void CoroutineScheduler::destroy(std::coroutine_handle<> handle) {
    if (!handle) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = coroutines_.find(handle.address());
    if (it == coroutines_.end()) {
        return;  // 协程未注册，可能已被销毁
    }
    
    // 销毁协程
    if (!handle.done()) {
        handle.destroy();
    } else {
        handle.destroy();
    }
    
    // 从映射表中移除
    coroutines_.erase(it);
}

CoroutineState CoroutineScheduler::get_state(std::coroutine_handle<> handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = coroutines_.find(handle.address());
    if (it == coroutines_.end()) {
        throw std::runtime_error("Coroutine not found in scheduler");
    }
    
    return it->second.state;
}

void* CoroutineScheduler::allocate_frame(size_t size) {
    return CoroutineMemoryPool::instance().allocate(size);
}

void CoroutineScheduler::deallocate_frame(void* ptr, size_t size) {
    CoroutineMemoryPool::instance().deallocate(ptr, size);
}

}  // namespace frpc
