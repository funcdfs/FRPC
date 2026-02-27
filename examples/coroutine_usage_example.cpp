/**
 * @file coroutine_usage_example.cpp
 * @brief RpcTask 协程返回类型使用示例
 * 
 * 本示例演示了 RpcTask 的各种使用方式，包括：
 * - 基本的协程定义和调用
 * - co_await 语法的使用
 * - 链式协程调用
 * - 异常处理
 * - 手动控制协程执行
 * 
 * 验证需求：2.1, 2.4, 15.1, 15.3
 */

#include "frpc/coroutine.h"
#include <iostream>
#include <string>
#include <vector>

using namespace frpc;

// ============================================================================
// 示例 1: 基本协程使用
// ============================================================================

/**
 * @brief 简单的协程函数，返回整数
 * 
 * 这是最基本的协程示例，演示了如何定义和使用返回值的协程。
 */
RpcTask<int> simple_add(int a, int b) {
    // 协程体：执行计算
    int result = a + b;
    
    // 使用 co_return 返回结果
    co_return result;
}

void example_basic_usage() {
    std::cout << "\n=== 示例 1: 基本协程使用 ===" << std::endl;
    
    // 调用协程函数，返回 RpcTask 对象
    auto task = simple_add(10, 20);
    
    // 使用 get() 方法获取结果（阻塞直到协程完成）
    int result = task.get();
    
    std::cout << "10 + 20 = " << result << std::endl;
}

// ============================================================================
// 示例 2: 使用 co_await 语法
// ============================================================================

/**
 * @brief 异步获取用户信息
 * 
 * 模拟异步操作，演示协程如何表示异步任务。
 */
RpcTask<std::string> fetch_user_name(int user_id) {
    // 在实际应用中，这里可能是网络请求或数据库查询
    co_return "User" + std::to_string(user_id);
}

/**
 * @brief 异步获取用户年龄
 */
RpcTask<int> fetch_user_age(int user_id) {
    // 模拟异步操作
    co_return 20 + user_id;
}

/**
 * @brief 组合多个异步操作
 * 
 * 演示如何使用 co_await 等待其他协程完成，实现同步代码风格的异步逻辑。
 * 这是 FRPC 框架的核心特性：将异步回调转化为同步编写范式。
 */
RpcTask<std::string> get_user_info(int user_id) {
    // 使用 co_await 等待异步操作完成
    // 协程会在此处挂起，直到 fetch_user_name 完成
    std::string name = co_await fetch_user_name(user_id);
    
    // 继续执行，等待下一个异步操作
    int age = co_await fetch_user_age(user_id);
    
    // 组合结果并返回
    co_return name + " is " + std::to_string(age) + " years old";
}

void example_co_await() {
    std::cout << "\n=== 示例 2: 使用 co_await 语法 ===" << std::endl;
    
    // 调用组合协程
    auto task = get_user_info(5);
    
    // 获取最终结果
    std::string info = task.get();
    
    std::cout << "User info: " << info << std::endl;
    // 输出: User info: User5 is 25 years old
}

// ============================================================================
// 示例 3: 链式协程调用
// ============================================================================

/**
 * @brief 第一步：验证用户
 */
RpcTask<bool> authenticate_user(const std::string& username, const std::string& password) {
    // 模拟身份验证
    co_return username == "admin" && password == "secret";
}

/**
 * @brief 第二步：获取用户权限
 */
RpcTask<std::vector<std::string>> get_user_permissions(const std::string& username) {
    // 模拟获取权限列表
    std::vector<std::string> permissions = {"read", "write", "execute"};
    co_return permissions;
}

/**
 * @brief 第三步：执行操作
 */
RpcTask<std::string> perform_operation(const std::vector<std::string>& permissions, const std::string& operation) {
    // 检查权限
    for (const auto& perm : permissions) {
        if (perm == operation) {
            co_return "Operation '" + operation + "' succeeded";
        }
    }
    co_return "Operation '" + operation + "' failed: permission denied";
}

/**
 * @brief 完整的业务流程：认证 -> 授权 -> 执行
 * 
 * 演示链式协程调用，每一步都依赖前一步的结果。
 * 使用同步代码风格编写复杂的异步业务逻辑。
 */
RpcTask<std::string> secure_operation(const std::string& username, const std::string& password, const std::string& operation) {
    // 第一步：认证
    bool authenticated = co_await authenticate_user(username, password);
    if (!authenticated) {
        co_return "Authentication failed";
    }
    
    // 第二步：获取权限
    auto permissions = co_await get_user_permissions(username);
    
    // 第三步：执行操作
    std::string result = co_await perform_operation(permissions, operation);
    
    co_return result;
}

void example_chained_coroutines() {
    std::cout << "\n=== 示例 3: 链式协程调用 ===" << std::endl;
    
    // 成功的情况
    auto task1 = secure_operation("admin", "secret", "write");
    std::cout << task1.get() << std::endl;
    // 输出: Operation 'write' succeeded
    
    // 认证失败的情况
    auto task2 = secure_operation("admin", "wrong", "write");
    std::cout << task2.get() << std::endl;
    // 输出: Authentication failed
    
    // 权限不足的情况
    auto task3 = secure_operation("admin", "secret", "delete");
    std::cout << task3.get() << std::endl;
    // 输出: Operation 'delete' failed: permission denied
}

// ============================================================================
// 示例 4: 异常处理
// ============================================================================

/**
 * @brief 可能抛出异常的协程
 */
RpcTask<int> risky_operation(int value) {
    if (value < 0) {
        throw std::invalid_argument("Value must be non-negative");
    }
    co_return value * 2;
}

/**
 * @brief 处理异常的协程
 * 
 * 演示如何在协程中捕获和处理异常。
 * 异常会通过 co_await 传播，可以使用标准的 try-catch 语法。
 */
RpcTask<int> safe_operation(int value) {
    try {
        // 尝试执行可能失败的操作
        int result = co_await risky_operation(value);
        co_return result;
    } catch (const std::invalid_argument& e) {
        // 捕获异常并返回错误码
        std::cerr << "Caught exception: " << e.what() << std::endl;
        co_return -1;
    }
}

void example_exception_handling() {
    std::cout << "\n=== 示例 4: 异常处理 ===" << std::endl;
    
    // 正常情况
    auto task1 = safe_operation(10);
    std::cout << "Result for 10: " << task1.get() << std::endl;
    // 输出: Result for 10: 20
    
    // 异常情况
    auto task2 = safe_operation(-5);
    std::cout << "Result for -5: " << task2.get() << std::endl;
    // 输出: Caught exception: Value must be non-negative
    //       Result for -5: -1
}

// ============================================================================
// 示例 5: 手动控制协程执行
// ============================================================================

/**
 * @brief 多步骤协程
 * 
 * 演示协程的挂起和恢复机制。
 */
RpcTask<int> multi_step_task() {
    std::cout << "  Step 1: Starting..." << std::endl;
    
    // 第一个挂起点
    co_await std::suspend_always{};
    std::cout << "  Step 2: Resumed after first suspend" << std::endl;
    
    // 第二个挂起点
    co_await std::suspend_always{};
    std::cout << "  Step 3: Resumed after second suspend" << std::endl;
    
    // 完成
    co_return 42;
}

void example_manual_control() {
    std::cout << "\n=== 示例 5: 手动控制协程执行 ===" << std::endl;
    
    // 创建协程（此时协程已创建但未开始执行）
    auto task = multi_step_task();
    
    std::cout << "Coroutine created, done: " << task.done() << std::endl;
    
    // 第一次恢复：执行到第一个挂起点
    std::cout << "Resuming first time..." << std::endl;
    task.resume();
    std::cout << "After first resume, done: " << task.done() << std::endl;
    
    // 第二次恢复：执行到第二个挂起点
    std::cout << "Resuming second time..." << std::endl;
    task.resume();
    std::cout << "After second resume, done: " << task.done() << std::endl;
    
    // 第三次恢复：执行完成
    std::cout << "Resuming third time..." << std::endl;
    task.resume();
    std::cout << "After third resume, done: " << task.done() << std::endl;
    
    // 获取结果
    int result = task.get();
    std::cout << "Final result: " << result << std::endl;
}

// ============================================================================
// 示例 6: void 协程
// ============================================================================

/**
 * @brief 不返回值的协程
 * 
 * 演示 RpcTask<void> 的使用。
 */
RpcTask<void> print_message(const std::string& message) {
    std::cout << "  Message: " << message << std::endl;
    co_return;  // void 协程使用 co_return; 或省略
}

/**
 * @brief 组合多个 void 协程
 */
RpcTask<void> print_multiple_messages() {
    co_await print_message("Hello");
    co_await print_message("World");
    co_await print_message("from FRPC!");
}

void example_void_coroutine() {
    std::cout << "\n=== 示例 6: void 协程 ===" << std::endl;
    
    auto task = print_multiple_messages();
    task.get();  // void 协程的 get() 不返回值
}

// ============================================================================
// 示例 7: Awaitable 接口直接使用
// ============================================================================

/**
 * @brief 演示 awaitable 接口的直接使用
 * 
 * 虽然通常使用 co_await 语法，但也可以直接调用 awaitable 接口方法。
 */
void example_awaitable_interface() {
    std::cout << "\n=== 示例 7: Awaitable 接口直接使用 ===" << std::endl;
    
    auto task = simple_add(5, 10);
    
    // 检查是否已完成
    std::cout << "await_ready (before): " << task.await_ready() << std::endl;
    
    // 手动恢复直到完成
    while (!task.await_ready()) {
        task.resume();
    }
    
    std::cout << "await_ready (after): " << task.await_ready() << std::endl;
    
    // 获取结果
    int result = task.await_resume();
    std::cout << "Result: " << result << std::endl;
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "RpcTask 协程返回类型使用示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        example_basic_usage();
        example_co_await();
        example_chained_coroutines();
        example_exception_handling();
        example_manual_control();
        example_void_coroutine();
        example_awaitable_interface();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "所有示例执行完成！" << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
