#include <gtest/gtest.h>
#include "frpc/coroutine.h"
#include <string>

using namespace frpc;

/**
 * @brief 测试 RpcTask 的 awaitable 接口
 * 
 * 验证需求 2.1, 2.4, 15.1, 15.3
 */

// 简单的协程函数，返回整数
RpcTask<int> simple_coroutine() {
    co_return 42;
}

// 简单的协程函数，返回字符串
RpcTask<std::string> string_coroutine() {
    co_return "hello";
}

// void 协程函数
RpcTask<void> void_coroutine() {
    co_return;
}

// 使用 co_await 的协程
RpcTask<int> awaiting_coroutine() {
    auto task = simple_coroutine();
    int result = co_await task;
    co_return result * 2;
}

// 链式 co_await
RpcTask<std::string> chained_await() {
    auto task1 = string_coroutine();
    std::string result1 = co_await task1;
    
    auto task2 = string_coroutine();
    std::string result2 = co_await task2;
    
    co_return result1 + " " + result2;
}

// 测试 await_ready 方法
TEST(RpcTaskAwaitableTest, AwaitReady) {
    auto task = simple_coroutine();
    
    // 协程刚创建时未完成
    EXPECT_FALSE(task.await_ready());
    
    // 恢复协程直到完成
    while (!task.done()) {
        task.resume();
    }
    
    // 协程完成后 await_ready 应该返回 true
    EXPECT_TRUE(task.await_ready());
}

// 测试 await_resume 方法
TEST(RpcTaskAwaitableTest, AwaitResume) {
    auto task = simple_coroutine();
    
    // 恢复协程直到完成
    while (!task.done()) {
        task.resume();
    }
    
    // 调用 await_resume 获取结果
    int result = task.await_resume();
    EXPECT_EQ(result, 42);
}

// 测试 co_await 语法
TEST(RpcTaskAwaitableTest, CoAwaitSyntax) {
    auto task = awaiting_coroutine();
    
    // 获取结果（内部使用 co_await）
    int result = task.get();
    
    // 验证结果
    EXPECT_EQ(result, 84);  // 42 * 2
}

// 测试链式 co_await
TEST(RpcTaskAwaitableTest, ChainedCoAwait) {
    auto task = chained_await();
    
    // 获取结果
    std::string result = task.get();
    
    // 验证结果
    EXPECT_EQ(result, "hello hello");
}

// 测试 void 协程的 awaitable 接口
TEST(RpcTaskAwaitableTest, VoidCoroutineAwaitable) {
    auto task = void_coroutine();
    
    // 协程刚创建时未完成
    EXPECT_FALSE(task.await_ready());
    
    // 恢复协程直到完成
    while (!task.done()) {
        task.resume();
    }
    
    // 协程完成后 await_ready 应该返回 true
    EXPECT_TRUE(task.await_ready());
    
    // 调用 await_resume（不返回值）
    EXPECT_NO_THROW(task.await_resume());
}

// 测试异常传播
RpcTask<int> throwing_coroutine() {
    throw std::runtime_error("Test exception");
    co_return 0;  // 不会执行到这里
}

RpcTask<int> catching_coroutine() {
    try {
        auto task = throwing_coroutine();
        int result = co_await task;
        co_return result;
    } catch (const std::runtime_error& e) {
        // 捕获异常并返回错误码
        co_return -1;
    }
}

TEST(RpcTaskAwaitableTest, ExceptionPropagation) {
    auto task = catching_coroutine();
    
    // 获取结果
    int result = task.get();
    
    // 验证异常被正确捕获
    EXPECT_EQ(result, -1);
}

// 测试多个协程并发
RpcTask<int> concurrent_task(int value) {
    co_return value * 2;
}

RpcTask<int> concurrent_caller() {
    auto task1 = concurrent_task(10);
    auto task2 = concurrent_task(20);
    auto task3 = concurrent_task(30);
    
    int result1 = co_await task1;
    int result2 = co_await task2;
    int result3 = co_await task3;
    
    co_return result1 + result2 + result3;
}

TEST(RpcTaskAwaitableTest, ConcurrentTasks) {
    auto task = concurrent_caller();
    
    // 获取结果
    int result = task.get();
    
    // 验证结果：(10*2) + (20*2) + (30*2) = 120
    EXPECT_EQ(result, 120);
}
