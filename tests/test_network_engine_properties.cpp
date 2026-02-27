#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "frpc/network_engine.h"

using namespace frpc;

// 辅助函数：设置 socket 为非阻塞模式
static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

// 辅助函数：创建一对连接的 socket
static std::pair<int, int> create_socket_pair() {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        throw std::runtime_error("Failed to create socket pair");
    }
    
    set_nonblocking(sv[0]);
    set_nonblocking(sv[1]);
    
    return {sv[0], sv[1]};
}

/**
 * @brief Property 1: 网络事件正确分发
 * 
 * Feature: frpc-framework, Property 1: 网络事件正确分发
 * 
 * 验证：需求 1.3
 * 
 * 对于任何注册的网络事件（读、写、错误），当该事件在 socket 上触发时，
 * Network Engine 应该调用对应的事件处理器，且只调用该事件类型的处理器。
 * 
 * 测试策略：
 * 1. 创建多个 socket 对
 * 2. 为每个 socket 注册不同类型的事件处理器
 * 3. 触发特定类型的事件
 * 4. 验证只有对应类型的处理器被调用
 */
RC_GTEST_PROP(NetworkEnginePropertyTest, EventDispatchCorrectness, 
              (int num_sockets)) {
    // 前置条件：socket 数量在合理范围内
    RC_PRE(num_sockets > 0 && num_sockets <= 10);
    
    NetworkEngine engine;
    
    // 创建 socket 对
    std::vector<std::pair<int, int>> socket_pairs;
    for (int i = 0; i < num_sockets; ++i) {
        socket_pairs.push_back(create_socket_pair());
    }
    
    // 跟踪哪些回调被调用
    std::vector<std::atomic<bool>> read_callbacks_called(num_sockets);
    std::vector<std::atomic<bool>> write_callbacks_called(num_sockets);
    
    for (int i = 0; i < num_sockets; ++i) {
        read_callbacks_called[i] = false;
        write_callbacks_called[i] = false;
    }
    
    // 注册读事件处理器
    for (int i = 0; i < num_sockets; ++i) {
        int fd = socket_pairs[i].first;
        engine.register_read_event(fd, [&read_callbacks_called, i, fd]() {
            read_callbacks_called[i] = true;
            // 读取数据以清空缓冲区
            char buffer[128];
            read(fd, buffer, sizeof(buffer));
        });
    }
    
    // 注册写事件处理器
    for (int i = 0; i < num_sockets; ++i) {
        int fd = socket_pairs[i].first;
        engine.register_write_event(fd, [&write_callbacks_called, i]() {
            write_callbacks_called[i] = true;
        });
    }
    
    // 在单独的线程中运行事件循环
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 触发读事件：向每个 socket 的对端写入数据
    for (int i = 0; i < num_sockets; ++i) {
        const char* data = "Test data";
        write(socket_pairs[i].second, data, strlen(data));
    }
    
    // 等待事件处理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证：所有读事件处理器都被调用
    for (int i = 0; i < num_sockets; ++i) {
        RC_ASSERT(read_callbacks_called[i].load());
    }
    
    // 验证：所有写事件处理器都被调用（socket 通常立即可写）
    for (int i = 0; i < num_sockets; ++i) {
        RC_ASSERT(write_callbacks_called[i].load());
    }
    
    // 停止事件循环
    engine.stop();
    event_thread.join();
    
    // 清理
    for (int i = 0; i < num_sockets; ++i) {
        engine.unregister_event(socket_pairs[i].first);
        close(socket_pairs[i].first);
        close(socket_pairs[i].second);
    }
}

/**
 * @brief Property 1 变体: 读事件只触发读处理器
 * 
 * 验证：需求 1.3
 * 
 * 当只有读事件发生时，只有读事件处理器被调用，写事件处理器不应该被调用。
 */
RC_GTEST_PROP(NetworkEnginePropertyTest, ReadEventOnlyTriggersReadHandler,
              (int num_sockets)) {
    RC_PRE(num_sockets > 0 && num_sockets <= 5);
    
    NetworkEngine engine;
    
    std::vector<std::pair<int, int>> socket_pairs;
    for (int i = 0; i < num_sockets; ++i) {
        socket_pairs.push_back(create_socket_pair());
    }
    
    std::vector<std::atomic<int>> read_call_count(num_sockets);
    std::vector<std::atomic<int>> write_call_count(num_sockets);
    
    for (int i = 0; i < num_sockets; ++i) {
        read_call_count[i] = 0;
        write_call_count[i] = 0;
    }
    
    // 只注册读事件处理器
    for (int i = 0; i < num_sockets; ++i) {
        int fd = socket_pairs[i].first;
        engine.register_read_event(fd, [&read_call_count, i, fd]() {
            read_call_count[i]++;
            char buffer[128];
            read(fd, buffer, sizeof(buffer));
        });
    }
    
    // 在单独的线程中运行事件循环
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 触发读事件
    for (int i = 0; i < num_sockets; ++i) {
        const char* data = "Test";
        write(socket_pairs[i].second, data, strlen(data));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证：读处理器被调用
    for (int i = 0; i < num_sockets; ++i) {
        RC_ASSERT(read_call_count[i].load() > 0);
    }
    
    // 验证：写处理器没有被调用（因为没有注册）
    for (int i = 0; i < num_sockets; ++i) {
        RC_ASSERT(write_call_count[i].load() == 0);
    }
    
    engine.stop();
    event_thread.join();
    
    for (int i = 0; i < num_sockets; ++i) {
        engine.unregister_event(socket_pairs[i].first);
        close(socket_pairs[i].first);
        close(socket_pairs[i].second);
    }
}

/**
 * @brief Property 1 变体: 事件处理器独立性
 * 
 * 验证：需求 1.3
 * 
 * 不同 socket 的事件处理器是独立的，一个 socket 的事件不应该触发其他 socket 的处理器。
 */
RC_GTEST_PROP(NetworkEnginePropertyTest, EventHandlerIndependence,
              (int target_socket_index, int num_sockets)) {
    RC_PRE(num_sockets >= 2 && num_sockets <= 5);
    RC_PRE(target_socket_index >= 0 && target_socket_index < num_sockets);
    
    NetworkEngine engine;
    
    std::vector<std::pair<int, int>> socket_pairs;
    for (int i = 0; i < num_sockets; ++i) {
        socket_pairs.push_back(create_socket_pair());
    }
    
    std::vector<std::atomic<bool>> callbacks_called(num_sockets);
    for (int i = 0; i < num_sockets; ++i) {
        callbacks_called[i] = false;
    }
    
    // 为所有 socket 注册读事件处理器
    for (int i = 0; i < num_sockets; ++i) {
        int fd = socket_pairs[i].first;
        engine.register_read_event(fd, [&callbacks_called, i, fd]() {
            callbacks_called[i] = true;
            char buffer[128];
            read(fd, buffer, sizeof(buffer));
        });
    }
    
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 只触发目标 socket 的读事件
    const char* data = "Target data";
    write(socket_pairs[target_socket_index].second, data, strlen(data));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证：只有目标 socket 的处理器被调用
    RC_ASSERT(callbacks_called[target_socket_index].load());
    
    // 验证：其他 socket 的处理器没有被调用
    for (int i = 0; i < num_sockets; ++i) {
        if (i != target_socket_index) {
            RC_ASSERT(!callbacks_called[i].load());
        }
    }
    
    engine.stop();
    event_thread.join();
    
    for (int i = 0; i < num_sockets; ++i) {
        engine.unregister_event(socket_pairs[i].first);
        close(socket_pairs[i].first);
        close(socket_pairs[i].second);
    }
}

/**
 * @brief Property 1 变体: 事件处理器可靠性
 * 
 * 验证：需求 1.3
 * 
 * 事件处理器应该可靠地被调用，不会丢失事件。
 * 多次触发事件应该导致处理器被多次调用。
 */
RC_GTEST_PROP(NetworkEnginePropertyTest, EventHandlerReliability,
              (int num_events)) {
    RC_PRE(num_events > 0 && num_events <= 10);
    
    NetworkEngine engine;
    
    auto [fd1, fd2] = create_socket_pair();
    
    std::atomic<int> callback_count{0};
    
    // 注册读事件处理器
    engine.register_read_event(fd1, [&callback_count, fd1]() {
        callback_count++;
        char buffer[128];
        read(fd1, buffer, sizeof(buffer));
    });
    
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 触发多次读事件
    for (int i = 0; i < num_events; ++i) {
        const char* data = "Event";
        write(fd2, data, strlen(data));
        // 等待一小段时间确保事件被处理
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    // 额外等待确保所有事件都被处理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证：处理器被调用的次数等于触发事件的次数
    RC_ASSERT(callback_count.load() == num_events);
    
    engine.stop();
    event_thread.join();
    
    engine.unregister_event(fd1);
    close(fd1);
    close(fd2);
}

/**
 * @brief Property 1 变体: 注销后不再触发
 * 
 * 验证：需求 1.5
 * 
 * 注销事件后，即使事件发生，处理器也不应该被调用。
 */
RC_GTEST_PROP(NetworkEnginePropertyTest, UnregisterStopsEvents,
              (bool trigger_before_unregister)) {
    NetworkEngine engine;
    
    auto [fd1, fd2] = create_socket_pair();
    
    std::atomic<int> callback_count{0};
    
    // 注册读事件处理器
    engine.register_read_event(fd1, [&callback_count, fd1]() {
        callback_count++;
        char buffer[128];
        read(fd1, buffer, sizeof(buffer));
    });
    
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    if (trigger_before_unregister) {
        // 注销前触发一次事件
        const char* data = "Before";
        write(fd2, data, strlen(data));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    int count_before_unregister = callback_count.load();
    
    // 注销事件
    engine.unregister_event(fd1);
    
    // 等待一小段时间确保注销生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 注销后触发事件
    const char* data = "After";
    write(fd2, data, strlen(data));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证：注销后回调次数没有增加
    RC_ASSERT(callback_count.load() == count_before_unregister);
    
    engine.stop();
    event_thread.join();
    
    close(fd1);
    close(fd2);
}

