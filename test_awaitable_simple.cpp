#include "include/frpc/coroutine.h"
#include <iostream>
#include <cassert>

using namespace frpc;

// 简单的协程函数，返回整数
RpcTask<int> simple_coroutine() {
    co_return 42;
}

// 使用 co_await 的协程
RpcTask<int> awaiting_coroutine() {
    auto task = simple_coroutine();
    int result = co_await task;
    co_return result * 2;
}

// void 协程函数
RpcTask<void> void_coroutine() {
    co_return;
}

// 链式 co_await
RpcTask<int> chained_await() {
    auto task1 = simple_coroutine();
    int result1 = co_await task1;
    
    auto task2 = simple_coroutine();
    int result2 = co_await task2;
    
    co_return result1 + result2;
}

int main() {
    std::cout << "Testing RpcTask awaitable interface..." << std::endl;
    
    // Test 1: await_ready
    {
        std::cout << "Test 1: await_ready... ";
        auto task = simple_coroutine();
        assert(!task.await_ready());  // 协程刚创建时未完成
        
        while (!task.done()) {
            task.resume();
        }
        
        assert(task.await_ready());  // 协程完成后应该返回 true
        std::cout << "PASSED" << std::endl;
    }
    
    // Test 2: await_resume
    {
        std::cout << "Test 2: await_resume... ";
        auto task = simple_coroutine();
        
        while (!task.done()) {
            task.resume();
        }
        
        int result = task.await_resume();
        assert(result == 42);
        std::cout << "PASSED" << std::endl;
    }
    
    // Test 3: co_await syntax
    {
        std::cout << "Test 3: co_await syntax... ";
        auto task = awaiting_coroutine();
        int result = task.get();
        assert(result == 84);  // 42 * 2
        std::cout << "PASSED" << std::endl;
    }
    
    // Test 4: chained co_await
    {
        std::cout << "Test 4: chained co_await... ";
        auto task = chained_await();
        int result = task.get();
        assert(result == 84);  // 42 + 42
        std::cout << "PASSED" << std::endl;
    }
    
    // Test 5: void coroutine awaitable
    {
        std::cout << "Test 5: void coroutine awaitable... ";
        auto task = void_coroutine();
        assert(!task.await_ready());
        
        while (!task.done()) {
            task.resume();
        }
        
        assert(task.await_ready());
        task.await_resume();  // 不应该抛出异常
        std::cout << "PASSED" << std::endl;
    }
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
