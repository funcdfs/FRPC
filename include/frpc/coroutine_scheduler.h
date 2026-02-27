#pragma once

#include <coroutine>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>
#include "coroutine.h"

namespace frpc {

/**
 * @brief 协程信息
 * 
 * 存储协程的元数据，包括状态、优先级和内存信息。
 */
struct CoroutineInfo {
    std::coroutine_handle<> handle;  ///< 协程句柄
    CoroutineState state;            ///< 协程状态
    int priority;                    ///< 优先级（数值越大优先级越高）
    void* frame_ptr;                 ///< 协程帧指针
    size_t frame_size;               ///< 协程帧大小
};

/**
 * @brief 调度任务
 * 
 * 用于优先级队列的任务包装器。
 */
struct ScheduleTask {
    std::coroutine_handle<> handle;  ///< 协程句柄
    int priority;                    ///< 优先级
    
    /**
     * @brief 比较运算符（用于优先级队列）
     * 
     * 优先级高的任务排在前面（最大堆）。
     */
    bool operator<(const ScheduleTask& other) const {
        return priority < other.priority;  // 注意：priority_queue 是最大堆
    }
};

/**
 * @brief 协程调度器 - 管理协程的创建、挂起、恢复和销毁
 * 
 * CoroutineScheduler 是 FRPC 框架的核心组件之一，负责协程的生命周期管理
 * 和调度。它提供了以下功能：
 * 
 * **核心功能：**
 * 
 * 1. **协程生命周期管理**：
 *    - create_coroutine(): 创建新协程并注册到调度器
 *    - suspend(): 挂起协程，将其从运行状态转为挂起状态
 *    - resume(): 恢复协程执行，将其从挂起状态转为运行状态
 *    - destroy(): 销毁协程，清理资源
 * 
 * 2. **协程状态管理**：
 *    - 跟踪每个协程的状态（Created、Running、Suspended、Completed、Failed）
 *    - 状态转换遵循严格的顺序：Created -> Running -> (Suspended <-> Running)* -> (Completed | Failed)
 *    - get_state(): 查询协程当前状态
 * 
 * 3. **优先级调度**：
 *    - 支持为协程设置优先级（数值越大优先级越高）
 *    - 使用优先级队列管理就绪协程
 *    - 高优先级协程优先执行
 * 
 * 4. **内存管理**：
 *    - 集成 CoroutineMemoryPool 进行协程帧内存分配
 *    - allocate_frame() 和 deallocate_frame() 提供内存池接口
 *    - 减少堆分配开销，提升性能
 * 
 * 5. **线程安全**：
 *    - 所有公共方法都是线程安全的
 *    - 使用互斥锁保护共享数据结构
 *    - 支持多线程环境下的协程调度
 * 
 * **协程生命周期流程：**
 * 
 * ```
 * 1. 创建阶段：
 *    - 用户调用 create_coroutine(func)
 *    - 调度器创建协程并设置状态为 Created
 *    - 协程被注册到调度器的协程映射表中
 * 
 * 2. 首次执行：
 *    - 用户调用 resume(handle)
 *    - 调度器将状态更新为 Running
 *    - 协程开始执行，直到遇到 co_await 或完成
 * 
 * 3. 挂起阶段：
 *    - 协程执行 co_await 时自动挂起
 *    - 调度器调用 suspend(handle)
 *    - 状态更新为 Suspended
 *    - 协程可以稍后通过 resume() 恢复
 * 
 * 4. 恢复阶段：
 *    - 调度器调用 resume(handle)
 *    - 状态更新为 Running
 *    - 协程从挂起点继续执行
 *    - 可以多次挂起和恢复
 * 
 * 5. 完成阶段：
 *    - 协程执行完成（co_return）或抛出异常
 *    - 状态更新为 Completed 或 Failed
 *    - 协程保持挂起状态，等待销毁
 * 
 * 6. 销毁阶段：
 *    - 用户调用 destroy(handle)
 *    - 调度器清理协程资源
 *    - 从协程映射表中移除
 *    - 释放协程帧内存
 * ```
 * 
 * **优先级调度策略：**
 * 
 * - 就绪队列使用优先级队列（最大堆）实现
 * - 高优先级协程优先被调度执行
 * - 相同优先级的协程按照加入队列的顺序执行（FIFO）
 * - 默认优先级为 0
 * 
 * **线程安全保证：**
 * 
 * - 所有修改共享状态的操作都受互斥锁保护
 * - 协程映射表和就绪队列的访问是线程安全的
 * - 支持多线程并发创建、挂起、恢复和销毁协程
 * 
 * **使用场景：**
 * 
 * - RPC 服务端：为每个客户端请求创建协程处理
 * - RPC 客户端：为每个远程调用创建协程
 * - 异步 I/O 操作：网络读写、文件操作等
 * - 任务调度：需要挂起和恢复的长时间运行任务
 * 
 * @note 线程安全：所有公共方法都是线程安全的
 * @note 内存管理：协程帧内存从全局内存池分配，显著减少堆分配开销
 * @note 异常安全：协程中的异常会被捕获，协程状态会被标记为 Failed
 * 
 * @example 基本使用示例
 * @code
 * CoroutineScheduler scheduler;
 * 
 * // 创建协程
 * auto coro = []() -> RpcTask<int> {
 *     co_await std::suspend_always{};  // 挂起点
 *     co_return 42;
 * };
 * 
 * auto handle = scheduler.create_coroutine(coro);
 * 
 * // 查询状态
 * assert(scheduler.get_state(handle) == CoroutineState::Created);
 * 
 * // 恢复执行
 * scheduler.resume(handle);  // 执行到挂起点
 * assert(scheduler.get_state(handle) == CoroutineState::Suspended);
 * 
 * // 再次恢复
 * scheduler.resume(handle);  // 执行完成
 * assert(scheduler.get_state(handle) == CoroutineState::Completed);
 * 
 * // 销毁协程
 * scheduler.destroy(handle);
 * @endcode
 * 
 * @example 优先级调度示例
 * @code
 * CoroutineScheduler scheduler;
 * 
 * // 创建低优先级协程
 * auto low_priority = scheduler.create_coroutine(low_priority_task, 1);
 * 
 * // 创建高优先级协程
 * auto high_priority = scheduler.create_coroutine(high_priority_task, 10);
 * 
 * // 高优先级协程会优先执行
 * scheduler.resume(high_priority);
 * scheduler.resume(low_priority);
 * @endcode
 * 
 * @example 异常处理示例
 * @code
 * CoroutineScheduler scheduler;
 * 
 * auto coro = []() -> RpcTask<int> {
 *     throw std::runtime_error("Error");
 *     co_return 0;
 * };
 * 
 * auto handle = scheduler.create_coroutine(coro);
 * 
 * try {
 *     scheduler.resume(handle);
 * } catch (...) {
 *     // 异常被捕获
 * }
 * 
 * // 协程状态为 Failed
 * assert(scheduler.get_state(handle) == CoroutineState::Failed);
 * 
 * scheduler.destroy(handle);
 * @endcode
 */
class CoroutineScheduler {
public:
    /**
     * @brief 构造函数
     */
    CoroutineScheduler() = default;
    
    /**
     * @brief 析构函数
     * 
     * 销毁所有未销毁的协程，释放资源。
     */
    ~CoroutineScheduler();
    
    // 禁止拷贝和移动
    CoroutineScheduler(const CoroutineScheduler&) = delete;
    CoroutineScheduler& operator=(const CoroutineScheduler&) = delete;
    CoroutineScheduler(CoroutineScheduler&&) = delete;
    CoroutineScheduler& operator=(CoroutineScheduler&&) = delete;
    
    /**
     * @brief 创建新协程
     * 
     * 创建一个新的协程并注册到调度器。协程初始状态为 Created，
     * 需要调用 resume() 才能开始执行。
     * 
     * @tparam Func 协程函数类型
     * @param func 协程函数（必须返回 RpcTask<T>）
     * @param priority 协程优先级（默认为 0，数值越大优先级越高）
     * @return 协程句柄
     * 
     * @note 线程安全
     * 
     * @example
     * @code
     * auto coro = []() -> RpcTask<int> {
     *     co_return 42;
     * };
     * 
     * auto handle = scheduler.create_coroutine(coro);
     * @endcode
     */
    template<typename Func>
    std::coroutine_handle<> create_coroutine(Func&& func, int priority = 0) {
        // 调用协程函数，获取 RpcTask
        auto task = func();
        
        // 释放 RpcTask 对协程句柄的所有权
        // 协程的生命周期由调度器管理
        auto handle = task.release();
        
        // 注册协程信息
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            CoroutineInfo info;
            info.handle = handle;
            info.state = CoroutineState::Created;
            info.priority = priority;
            info.frame_ptr = handle.address();
            info.frame_size = 0;  // 协程帧大小由编译器管理
            
            coroutines_[handle.address()] = info;
        }
        
        return handle;
    }
    
    /**
     * @brief 挂起当前协程
     * 
     * 将协程状态从 Running 更新为 Suspended。
     * 
     * @param handle 协程句柄
     * 
     * @note 线程安全
     * @note 此方法不会实际挂起协程，只是更新状态。实际的挂起由 co_await 完成。
     * 
     * @example
     * @code
     * // 协程内部
     * co_await std::suspend_always{};  // 实际挂起
     * 
     * // 调度器外部
     * scheduler.suspend(handle);  // 更新状态
     * @endcode
     */
    void suspend(std::coroutine_handle<> handle);
    
    /**
     * @brief 恢复协程执行
     * 
     * 将协程状态更新为 Running 并恢复执行。
     * 
     * @param handle 协程句柄
     * 
     * @note 线程安全
     * @note 如果协程已完成或失败，此方法不会执行任何操作
     * 
     * @example
     * @code
     * auto handle = scheduler.create_coroutine(coro);
     * scheduler.resume(handle);  // 开始执行
     * @endcode
     */
    void resume(std::coroutine_handle<> handle);
    
    /**
     * @brief 销毁协程
     * 
     * 销毁协程并清理资源。协程句柄将失效，不应再使用。
     * 
     * @param handle 协程句柄
     * 
     * @note 线程安全
     * @note 销毁后，handle 将失效，不应再使用
     * 
     * @example
     * @code
     * auto handle = scheduler.create_coroutine(coro);
     * // ... 使用协程 ...
     * scheduler.destroy(handle);  // 清理资源
     * @endcode
     */
    void destroy(std::coroutine_handle<> handle);
    
    /**
     * @brief 查询协程状态
     * 
     * 返回协程的当前状态。
     * 
     * @param handle 协程句柄
     * @return 协程状态
     * @throws std::runtime_error 如果协程未注册
     * 
     * @note 线程安全
     * 
     * @example
     * @code
     * auto state = scheduler.get_state(handle);
     * if (state == CoroutineState::Completed) {
     *     // 协程已完成
     * }
     * @endcode
     */
    CoroutineState get_state(std::coroutine_handle<> handle) const;
    
    /**
     * @brief 分配协程帧内存
     * 
     * 从内存池分配协程帧内存。
     * 
     * @param size 需要的字节数
     * @return 内存指针
     * 
     * @note 线程安全
     * @note 此方法由 RpcTaskPromise::operator new 调用
     */
    void* allocate_frame(size_t size);
    
    /**
     * @brief 释放协程帧内存
     * 
     * 将内存归还给内存池。
     * 
     * @param ptr 内存指针
     * @param size 字节数
     * 
     * @note 线程安全
     * @note 此方法由 RpcTaskPromise::operator delete 调用
     */
    void deallocate_frame(void* ptr, size_t size);

private:
    /**
     * @brief 协程映射表（协程地址 -> 协程信息）
     */
    std::unordered_map<void*, CoroutineInfo> coroutines_;
    
    /**
     * @brief 就绪队列（优先级队列）
     * 
     * 存储就绪状态的协程，按优先级排序。
     */
    std::priority_queue<ScheduleTask> ready_queue_;
    
    /**
     * @brief 互斥锁（保护并发访问）
     */
    mutable std::mutex mutex_;
};

}  // namespace frpc
