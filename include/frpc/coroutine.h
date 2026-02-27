#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include "memory_pool.h"

namespace frpc {

/**
 * @brief 协程状态枚举
 */
enum class CoroutineState {
    Created,    ///< 已创建，未开始执行
    Running,    ///< 正在执行
    Suspended,  ///< 已挂起
    Completed,  ///< 已完成
    Failed      ///< 执行失败
};

// Forward declaration
template<typename T>
class RpcTask;

/**
 * @brief 全局协程内存池
 * 
 * 用于协程帧的内存分配，避免频繁的堆分配。
 * 使用单例模式确保全局唯一。
 * 
 * @note 线程安全
 */
class CoroutineMemoryPool {
public:
    /**
     * @brief 获取全局实例
     */
    static CoroutineMemoryPool& instance() {
        static CoroutineMemoryPool pool;
        return pool;
    }
    
    /**
     * @brief 分配协程帧内存
     * @param size 需要的字节数
     * @return 内存指针
     */
    void* allocate(size_t size) {
        // 如果请求的大小超过内存池块大小，回退到标准堆分配
        void* ptr = pool_.allocate(size);
        if (!ptr) {
            return ::operator new(size);
        }
        return ptr;
    }
    
    /**
     * @brief 释放协程帧内存
     * @param ptr 内存指针
     * @param size 字节数
     */
    void deallocate(void* ptr, size_t size) {
        // 如果大小超过内存池块大小，使用标准堆释放
        if (size > pool_.block_size()) {
            ::operator delete(ptr);
        } else {
            pool_.deallocate(ptr);
        }
    }
    
    /**
     * @brief 获取内存池统计信息
     */
    MemoryPool::Stats get_stats() const {
        return pool_.get_stats();
    }

private:
    CoroutineMemoryPool() 
        : pool_(4096, 100) {}  // 每个块 4KB，初始 100 个块
    
    MemoryPool pool_;
};

/**
 * @brief RPC 任务的 Promise 类型
 * 
 * Promise 类型是 C++20 协程的核心组件，用于控制协程的行为和生命周期。
 * 它定义了协程在各个关键点的行为：
 * 
 * **协程控制流程：**
 * 
 * 1. **协程创建阶段**：
 *    - 编译器调用 operator new 分配协程帧内存（包含 Promise 对象和局部变量）
 *    - 构造 Promise 对象
 *    - 调用 get_return_object() 创建返回给调用者的 RpcTask 对象
 * 
 * 2. **初始挂起点**：
 *    - 调用 initial_suspend()，返回 std::suspend_always
 *    - 协程立即挂起，不执行协程体代码
 *    - 这允许调用者在协程开始执行前进行设置（如设置调度器）
 * 
 * 3. **协程执行阶段**：
 *    - 调用者调用 coroutine_handle.resume() 恢复协程
 *    - 协程体代码开始执行
 *    - 遇到 co_await 时可能再次挂起
 *    - 遇到 co_return 时调用 return_value() 保存返回值
 * 
 * 4. **异常处理**：
 *    - 如果协程体抛出未捕获的异常，调用 unhandled_exception()
 *    - Promise 保存异常指针，供后续重新抛出
 * 
 * 5. **最终挂起点**：
 *    - 协程执行完成后调用 final_suspend()，返回 std::suspend_always
 *    - 协程保持挂起状态，等待调用者销毁
 *    - 这允许调用者安全地访问协程结果
 * 
 * 6. **协程销毁阶段**：
 *    - 调用者调用 coroutine_handle.destroy()
 *    - 销毁 Promise 对象和局部变量
 *    - 调用 operator delete 释放协程帧内存
 * 
 * **内存管理优化：**
 * 
 * 通过自定义 operator new/delete，协程帧内存从内存池分配而非堆，
 * 显著减少内存分配开销（目标：减少 80% 的堆分配次数）。
 * 
 * @tparam T 协程返回值类型
 * 
 * @example 协程使用示例
 * @code
 * RpcTask<int> my_coroutine() {
 *     // 协程体
 *     co_await some_async_operation();
 *     co_return 42;
 * }
 * 
 * // 使用协程
 * auto task = my_coroutine();  // 协程创建但未执行
 * task.resume();               // 开始执行
 * int result = task.get();     // 获取结果
 * @endcode
 */
template<typename T>
struct RpcTaskPromise {
    /**
     * @brief 获取协程返回对象
     * 
     * 在协程创建时调用，返回给调用者的 RpcTask 对象。
     * RpcTask 封装了协程句柄，提供控制协程执行的接口。
     * 
     * @return RpcTask 对象
     */
    RpcTask<T> get_return_object();
    
    /**
     * @brief 初始挂起点 - 协程创建后立即挂起
     * 
     * 返回 std::suspend_always 表示协程创建后不自动执行，
     * 而是等待调用者显式调用 resume()。
     * 
     * 这种设计的优点：
     * - 允许调用者在协程开始前进行设置（如注册到调度器）
     * - 避免协程在构造函数中就开始执行导致的竞态条件
     * - 提供更精确的执行控制
     * 
     * @return std::suspend_always 表示总是挂起
     */
    std::suspend_always initial_suspend() noexcept { return {}; }
    
    /**
     * @brief 最终挂起点 - 协程完成后挂起，等待销毁
     * 
     * 返回 std::suspend_always 表示协程执行完成后保持挂起状态，
     * 而不是自动销毁。
     * 
     * 这种设计的优点：
     * - 允许调用者安全地访问协程结果（通过 Promise）
     * - 避免协程在结果被读取前就被销毁
     * - 调用者可以控制协程的销毁时机
     * 
     * @return std::suspend_always 表示总是挂起
     */
    std::suspend_always final_suspend() noexcept { return {}; }
    
    /**
     * @brief 处理 co_return 语句
     * 
     * 当协程执行 co_return value 时调用，保存返回值。
     * 返回值存储在 std::optional 中，以区分"有返回值"和"未返回"状态。
     * 
     * @param value 协程返回的值
     */
    void return_value(T value) {
        result_ = std::move(value);
    }
    
    /**
     * @brief 处理未捕获的异常
     * 
     * 当协程体抛出未捕获的异常时调用。
     * 保存异常指针，供后续通过 get_result() 重新抛出。
     * 
     * 这确保了异常不会丢失，调用者可以在获取结果时处理异常。
     */
    void unhandled_exception() {
        exception_ = std::current_exception();
    }
    
    /**
     * @brief 获取结果
     * 
     * 如果协程抛出了异常，重新抛出该异常。
     * 否则返回协程的返回值。
     * 
     * @return 协程返回值
     * @throws 协程执行过程中抛出的异常
     */
    T get_result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return std::move(*result_);
    }
    
    /**
     * @brief 自定义内存分配 - 使用协程内存池
     * 
     * 当编译器创建协程帧时调用此函数分配内存。
     * 从全局协程内存池分配，避免频繁的堆分配，提升性能。
     * 
     * **性能优化原理：**
     * - 内存池预分配固定大小的内存块，分配时只需从空闲链表获取
     * - 避免了 malloc/new 的系统调用开销
     * - 减少内存碎片，提高缓存局部性
     * - 目标：减少 80% 的堆分配次数
     * 
     * @param size 需要的字节数（协程帧大小）
     * @return 内存指针
     * 
     * @note 如果内存池无法满足请求（size 过大），回退到标准堆分配
     */
    void* operator new(size_t size) {
        return CoroutineMemoryPool::instance().allocate(size);
    }
    
    /**
     * @brief 自定义内存释放
     * 
     * 当协程销毁时调用此函数释放内存。
     * 将内存归还给内存池，供后续协程复用。
     * 
     * @param ptr 内存指针
     * @param size 字节数（协程帧大小）
     */
    void operator delete(void* ptr, size_t size) {
        CoroutineMemoryPool::instance().deallocate(ptr, size);
    }

private:
    std::optional<T> result_;           ///< 协程返回值
    std::exception_ptr exception_;      ///< 未捕获的异常
};

/**
 * @brief void 特化版本
 * 
 * 用于不返回值的协程（co_return; 或无 co_return）。
 * 
 * 与通用版本的区别：
 * - 使用 return_void() 而非 return_value()
 * - 没有 result_ 成员，只保存异常
 * - get_result() 不返回值，只重新抛出异常（如果有）
 * 
 * 协程控制流程与通用版本相同。
 */
template<>
struct RpcTaskPromise<void> {
    /**
     * @brief 获取协程返回对象
     */
    RpcTask<void> get_return_object();
    
    /**
     * @brief 初始挂起点 - 协程创建后立即挂起
     */
    std::suspend_always initial_suspend() noexcept { return {}; }
    
    /**
     * @brief 最终挂起点 - 协程完成后挂起，等待销毁
     */
    std::suspend_always final_suspend() noexcept { return {}; }
    
    /**
     * @brief 处理 co_return 语句（无返回值）
     * 
     * 当协程执行 co_return; 或到达函数末尾时调用。
     */
    void return_void() {}
    
    /**
     * @brief 处理未捕获的异常
     */
    void unhandled_exception() {
        exception_ = std::current_exception();
    }
    
    /**
     * @brief 获取结果（void 版本）
     * 
     * 如果协程抛出了异常，重新抛出该异常。
     * 
     * @throws 协程执行过程中抛出的异常
     */
    void get_result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
    
    /**
     * @brief 自定义内存分配 - 使用协程内存池
     * 
     * @param size 需要的字节数（协程帧大小）
     * @return 内存指针
     */
    void* operator new(size_t size) {
        return CoroutineMemoryPool::instance().allocate(size);
    }
    
    /**
     * @brief 自定义内存释放
     * 
     * @param ptr 内存指针
     * @param size 字节数（协程帧大小）
     */
    void operator delete(void* ptr, size_t size) {
        CoroutineMemoryPool::instance().deallocate(ptr, size);
    }

private:
    std::exception_ptr exception_;      ///< 未捕获的异常
};

/**
 * @brief RPC 任务协程返回类型
 * 
 * RpcTask 是 FRPC 框架中协程的返回类型，封装了协程句柄并提供了完整的
 * awaitable 接口，使其可以通过 co_await 语法使用。
 * 
 * **核心功能：**
 * 
 * 1. **协程生命周期管理**：
 *    - 自动管理协程句柄的生命周期
 *    - 支持移动语义，禁止拷贝
 *    - 析构时自动销毁协程
 * 
 * 2. **结果获取**：
 *    - get() 方法：阻塞直到协程完成并返回结果
 *    - 自动传播协程中的异常
 * 
 * 3. **Awaitable 接口**：
 *    - await_ready()：检查协程是否已完成
 *    - await_suspend()：挂起当前协程，恢复被等待的协程
 *    - await_resume()：获取协程结果
 *    - 使 RpcTask 可以通过 co_await 语法使用
 * 
 * 4. **手动控制**：
 *    - resume()：手动恢复协程执行
 *    - done()：检查协程是否完成
 * 
 * **使用场景：**
 * 
 * RpcTask 主要用于以下场景：
 * - 异步 RPC 调用
 * - 异步网络 I/O 操作
 * - 需要挂起和恢复的长时间运行任务
 * - 协程组合和链式调用
 * 
 * @tparam T 返回值类型
 * 
 * @example 基本使用示例
 * @code
 * // 定义一个简单的协程函数
 * RpcTask<int> compute() {
 *     // 执行一些计算
 *     co_return 42;
 * }
 * 
 * // 调用协程并获取结果
 * auto task = compute();
 * int result = task.get();  // 阻塞直到完成
 * std::cout << "Result: " << result << std::endl;
 * @endcode
 * 
 * @example 使用 co_await 语法
 * @code
 * // 定义异步操作
 * RpcTask<int> async_add(int a, int b) {
 *     co_return a + b;
 * }
 * 
 * // 在另一个协程中使用 co_await
 * RpcTask<int> caller() {
 *     // 使用 co_await 等待异步操作完成
 *     int result1 = co_await async_add(1, 2);
 *     int result2 = co_await async_add(3, 4);
 *     co_return result1 + result2;  // 返回 10
 * }
 * 
 * // 调用
 * auto task = caller();
 * int final_result = task.get();  // 10
 * @endcode
 * 
 * @example 链式协程调用
 * @code
 * RpcTask<std::string> fetch_user(int user_id) {
 *     // 模拟异步获取用户信息
 *     co_return "User" + std::to_string(user_id);
 * }
 * 
 * RpcTask<std::string> fetch_user_orders(const std::string& user) {
 *     // 模拟异步获取订单信息
 *     co_return user + "'s orders";
 * }
 * 
 * RpcTask<std::string> get_user_info(int user_id) {
 *     // 链式调用：先获取用户，再获取订单
 *     auto user = co_await fetch_user(user_id);
 *     auto orders = co_await fetch_user_orders(user);
 *     co_return orders;
 * }
 * 
 * // 使用
 * auto task = get_user_info(123);
 * std::string info = task.get();  // "User123's orders"
 * @endcode
 * 
 * @example 异常处理
 * @code
 * RpcTask<int> may_throw() {
 *     throw std::runtime_error("Something went wrong");
 *     co_return 0;  // 不会执行到这里
 * }
 * 
 * RpcTask<int> handle_error() {
 *     try {
 *         int result = co_await may_throw();
 *         co_return result;
 *     } catch (const std::runtime_error& e) {
 *         std::cerr << "Caught exception: " << e.what() << std::endl;
 *         co_return -1;  // 返回错误码
 *     }
 * }
 * 
 * // 使用
 * auto task = handle_error();
 * int result = task.get();  // -1
 * @endcode
 * 
 * @example 手动控制协程执行
 * @code
 * RpcTask<int> step_by_step() {
 *     co_await std::suspend_always{};  // 第一个挂起点
 *     co_await std::suspend_always{};  // 第二个挂起点
 *     co_return 42;
 * }
 * 
 * // 手动控制执行
 * auto task = step_by_step();
 * 
 * // 检查状态
 * assert(!task.done());
 * 
 * // 逐步恢复
 * task.resume();  // 执行到第一个挂起点
 * assert(!task.done());
 * 
 * task.resume();  // 执行到第二个挂起点
 * assert(!task.done());
 * 
 * task.resume();  // 执行完成
 * assert(task.done());
 * 
 * int result = task.get();  // 42
 * @endcode
 * 
 * @note 线程安全：RpcTask 本身不是线程安全的，不应该在多个线程间共享同一个 RpcTask 对象
 * @note 内存管理：协程帧内存从全局内存池分配，显著减少堆分配开销
 * @note 异常安全：协程中的异常会被捕获并在 get() 或 await_resume() 时重新抛出
 */
template<typename T = void>
class RpcTask {
public:
    using promise_type = RpcTaskPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;
    
    /**
     * @brief 构造函数
     */
    explicit RpcTask(handle_type handle) : handle_(handle) {}
    
    /**
     * @brief 析构函数 - 销毁协程
     */
    ~RpcTask() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    // 禁止拷贝
    RpcTask(const RpcTask&) = delete;
    RpcTask& operator=(const RpcTask&) = delete;
    
    // 允许移动
    RpcTask(RpcTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    RpcTask& operator=(RpcTask&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief 获取协程结果（阻塞直到完成）
     */
    T get() {
        if (!handle_) {
            throw std::runtime_error("Invalid coroutine handle");
        }
        
        // 恢复协程直到完成
        while (!handle_.done()) {
            handle_.resume();
        }
        
        return handle_.promise().get_result();
    }
    
    /**
     * @brief 检查协程是否完成
     */
    bool done() const {
        return handle_ && handle_.done();
    }
    
    /**
     * @brief 恢复协程执行
     */
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }
    
    /**
     * @brief 获取协程句柄
     */
    handle_type handle() const { return handle_; }
    
    /**
     * @brief 释放协程句柄的所有权
     * 
     * 将协程句柄的所有权转移给调用者，RpcTask 不再管理该协程的生命周期。
     * 调用此方法后，RpcTask 的 handle_ 将被设置为 nullptr。
     * 
     * @return 协程句柄
     * 
     * @note 调用者负责管理返回的协程句柄的生命周期
     * @note 调用此方法后，RpcTask 对象将失效，不应再使用
     */
    handle_type release() noexcept {
        auto h = handle_;
        handle_ = nullptr;
        return h;
    }
    
    // Awaitable interface - 使 RpcTask 可以被 co_await
    
    /**
     * @brief 检查协程是否已完成（Awaitable 接口）
     * 
     * 当使用 co_await task 时，编译器首先调用此方法检查操作是否已完成。
     * 
     * @return true 表示协程已完成，无需挂起当前协程
     * @return false 表示协程未完成，需要挂起当前协程
     * 
     * **工作原理：**
     * - 如果返回 true，编译器直接调用 await_resume() 获取结果，不挂起
     * - 如果返回 false，编译器调用 await_suspend() 挂起当前协程
     * 
     * @example
     * @code
     * RpcTask<int> task = async_operation();
     * int result = co_await task;  // 编译器调用 await_ready()
     * @endcode
     */
    bool await_ready() const noexcept {
        return handle_ && handle_.done();
    }
    
    /**
     * @brief 挂起当前协程（Awaitable 接口）
     * 
     * 当 await_ready() 返回 false 时调用，用于挂起当前协程。
     * 
     * @param awaiting_coroutine 等待此任务的协程句柄
     * 
     * **工作原理：**
     * - 此方法在当前协程（awaiting_coroutine）挂起后调用
     * - 我们恢复被等待的协程（handle_），让它继续执行
     * - 当被等待的协程完成后，我们恢复等待的协程（awaiting_coroutine）
     * 
     * **协程调度流程：**
     * 1. 协程 A 执行 co_await task_B
     * 2. task_B.await_ready() 返回 false（未完成）
     * 3. 协程 A 挂起
     * 4. task_B.await_suspend(handle_of_A) 被调用
     * 5. 我们恢复 task_B 的执行
     * 6. task_B 完成后，我们恢复协程 A
     * 7. 协程 A 调用 task_B.await_resume() 获取结果
     * 
     * @note 这是一个简化的实现，实际的调度器可能需要更复杂的逻辑
     * 
     * @example
     * @code
     * RpcTask<int> compute() {
     *     co_return 42;
     * }
     * 
     * RpcTask<void> caller() {
     *     auto task = compute();
     *     int result = co_await task;  // await_suspend() 在这里被调用
     *     std::cout << result << std::endl;
     * }
     * @endcode
     */
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) {
        // 简化实现：直接恢复被等待的协程直到完成
        // 实际的调度器可能会将协程加入就绪队列，由调度器统一管理
        while (handle_ && !handle_.done()) {
            handle_.resume();
        }
        
        // 被等待的协程完成后，恢复等待的协程
        // 注意：在实际的调度器中，这应该由调度器来完成
        awaiting_coroutine.resume();
    }
    
    /**
     * @brief 获取协程结果（Awaitable 接口）
     * 
     * 当协程完成后调用，返回协程的结果。
     * 
     * @return 协程返回值
     * @throws 协程执行过程中抛出的异常
     * 
     * **调用时机：**
     * - 如果 await_ready() 返回 true，立即调用
     * - 如果 await_ready() 返回 false，在 await_suspend() 恢复等待协程后调用
     * 
     * **异常处理：**
     * - 如果协程抛出了异常，此方法会重新抛出该异常
     * - 异常会传播到 co_await 表达式的调用者
     * 
     * @example
     * @code
     * RpcTask<int> task = async_operation();
     * try {
     *     int result = co_await task;  // await_resume() 返回结果
     * } catch (const std::exception& e) {
     *     // 处理协程抛出的异常
     * }
     * @endcode
     */
    T await_resume() {
        if (!handle_) {
            throw std::runtime_error("Invalid coroutine handle");
        }
        return handle_.promise().get_result();
    }

private:
    handle_type handle_;
};

/**
 * @brief void 特化版本
 */
template<>
class RpcTask<void> {
public:
    using promise_type = RpcTaskPromise<void>;
    using handle_type = std::coroutine_handle<promise_type>;
    
    explicit RpcTask(handle_type handle) : handle_(handle) {}
    
    ~RpcTask() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    RpcTask(const RpcTask&) = delete;
    RpcTask& operator=(const RpcTask&) = delete;
    
    RpcTask(RpcTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    RpcTask& operator=(RpcTask&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    void get() {
        if (!handle_) {
            throw std::runtime_error("Invalid coroutine handle");
        }
        
        while (!handle_.done()) {
            handle_.resume();
        }
        
        handle_.promise().get_result();
    }
    
    bool done() const {
        return handle_ && handle_.done();
    }
    
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }
    
    handle_type handle() const { return handle_; }
    
    /**
     * @brief 释放协程句柄的所有权
     * 
     * 将协程句柄的所有权转移给调用者，RpcTask 不再管理该协程的生命周期。
     * 调用此方法后，RpcTask 的 handle_ 将被设置为 nullptr。
     * 
     * @return 协程句柄
     * 
     * @note 调用者负责管理返回的协程句柄的生命周期
     * @note 调用此方法后，RpcTask 对象将失效，不应再使用
     */
    handle_type release() noexcept {
        auto h = handle_;
        handle_ = nullptr;
        return h;
    }
    
    // Awaitable interface - 使 RpcTask<void> 可以被 co_await
    
    /**
     * @brief 检查协程是否已完成（Awaitable 接口）
     * 
     * @return true 表示协程已完成，无需挂起当前协程
     * @return false 表示协程未完成，需要挂起当前协程
     */
    bool await_ready() const noexcept {
        return handle_ && handle_.done();
    }
    
    /**
     * @brief 挂起当前协程（Awaitable 接口）
     * 
     * @param awaiting_coroutine 等待此任务的协程句柄
     */
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) {
        // 简化实现：直接恢复被等待的协程直到完成
        while (handle_ && !handle_.done()) {
            handle_.resume();
        }
        
        // 被等待的协程完成后，恢复等待的协程
        awaiting_coroutine.resume();
    }
    
    /**
     * @brief 获取协程结果（Awaitable 接口）
     * 
     * void 版本不返回值，只重新抛出异常（如果有）。
     * 
     * @throws 协程执行过程中抛出的异常
     */
    void await_resume() {
        if (!handle_) {
            throw std::runtime_error("Invalid coroutine handle");
        }
        handle_.promise().get_result();
    }

private:
    handle_type handle_;
};

// Implementation of get_return_object() after RpcTask is defined
template<typename T>
inline RpcTask<T> RpcTaskPromise<T>::get_return_object() {
    return RpcTask<T>{std::coroutine_handle<RpcTaskPromise>::from_promise(*this)};
}

inline RpcTask<void> RpcTaskPromise<void>::get_return_object() {
    return RpcTask<void>{std::coroutine_handle<RpcTaskPromise>::from_promise(*this)};
}

}  // namespace frpc

