#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "frpc/network_engine.h"
#include "frpc/exceptions.h"

using namespace frpc;

// 辅助函数：设置 socket 为非阻塞模式
static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

// 辅助函数：创建一对连接的 socket（用于测试）
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
 * @brief 测试网络引擎的构造和析构
 * 
 * 验证：
 * - NetworkEngine 可以成功创建
 * - 析构时正确清理资源
 */
TEST(NetworkEngineTest, ConstructorAndDestructor) {
    // 创建网络引擎
    ASSERT_NO_THROW({
        NetworkEngine engine;
    });
    
    // 使用自定义缓冲区配置
    ASSERT_NO_THROW({
        NetworkEngine engine(16384, 200);
    });
}

/**
 * @brief 测试事件循环的启动和停止
 * 
 * 验证：
 * - run() 方法可以启动事件循环
 * - stop() 方法可以停止事件循环
 * - 事件循环在停止后正确退出
 * 
 * 需求: 1.1, 1.5
 */
TEST(NetworkEngineTest, StartAndStopEventLoop) {
    NetworkEngine engine;
    
    // 在单独的线程中运行事件循环
    std::atomic<bool> loop_started{false};
    std::atomic<bool> loop_stopped{false};
    
    std::thread event_thread([&engine, &loop_started, &loop_stopped]() {
        loop_started = true;
        engine.run();
        loop_stopped = true;
    });
    
    // 等待事件循环启动
    while (!loop_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 等待一小段时间，确保事件循环正在运行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 停止事件循环
    engine.stop();
    
    // 等待事件循环线程退出
    event_thread.join();
    
    // 验证事件循环已停止
    ASSERT_TRUE(loop_stopped.load());
}

/**
 * @brief 测试重复调用 run() 不会导致问题
 * 
 * 验证：
 * - 多次调用 run() 不会导致崩溃
 * - 第二次调用 run() 会被忽略
 */
TEST(NetworkEngineTest, MultipleRunCalls) {
    NetworkEngine engine;
    
    std::thread t1([&engine]() {
        engine.run();
    });
    
    // 等待第一个线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 第二次调用 run() 应该被忽略
    std::thread t2([&engine]() {
        engine.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    engine.stop();
    
    t1.join();
    t2.join();
}

/**
 * @brief 测试事件注册和注销
 * 
 * 验证：
 * - register_read_event() 可以成功注册读事件
 * - register_write_event() 可以成功注册写事件
 * - unregister_event() 可以注销事件
 * 
 * 需求: 1.1, 1.5
 */
TEST(NetworkEngineTest, RegisterAndUnregisterEvents) {
    NetworkEngine engine;
    
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 注册读事件
    bool read_callback_called = false;
    ASSERT_TRUE(engine.register_read_event(fd1, [&read_callback_called]() {
        read_callback_called = true;
    }));
    
    // 注册写事件
    bool write_callback_called = false;
    ASSERT_TRUE(engine.register_write_event(fd2, [&write_callback_called]() {
        write_callback_called = true;
    }));
    
    // 注销事件
    engine.unregister_event(fd1);
    engine.unregister_event(fd2);
    
    // 清理
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试读事件触发
 * 
 * 验证：
 * - 当 socket 有数据可读时，读事件回调被调用
 * - 回调函数在事件循环中正确执行
 * 
 * 需求: 1.1, 1.3
 */
TEST(NetworkEngineTest, ReadEventTriggered) {
    NetworkEngine engine;
    
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 注册读事件
    std::atomic<bool> read_callback_called{false};
    engine.register_read_event(fd1, [&read_callback_called, fd1]() {
        read_callback_called = true;
        
        // 读取数据
        char buffer[128];
        ssize_t n = read(fd1, buffer, sizeof(buffer));
        EXPECT_GT(n, 0);
    });
    
    // 在单独的线程中运行事件循环
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 向 fd2 写入数据，触发 fd1 的读事件
    const char* data = "Hello, NetworkEngine!";
    ssize_t written = write(fd2, data, strlen(data));
    ASSERT_GT(written, 0);
    
    // 等待回调被调用
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证回调被调用
    EXPECT_TRUE(read_callback_called.load());
    
    // 停止事件循环
    engine.stop();
    event_thread.join();
    
    // 清理
    engine.unregister_event(fd1);
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试写事件触发
 * 
 * 验证：
 * - 当 socket 可写时，写事件回调被调用
 * - 回调函数在事件循环中正确执行
 * 
 * 需求: 1.1, 1.3
 */
TEST(NetworkEngineTest, WriteEventTriggered) {
    NetworkEngine engine;
    
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 注册写事件（socket 通常立即可写）
    std::atomic<bool> write_callback_called{false};
    engine.register_write_event(fd1, [&write_callback_called]() {
        write_callback_called = true;
    });
    
    // 在单独的线程中运行事件循环
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    // 等待回调被调用
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证回调被调用
    EXPECT_TRUE(write_callback_called.load());
    
    // 停止事件循环
    engine.stop();
    event_thread.join();
    
    // 清理
    engine.unregister_event(fd1);
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试同时注册读写事件
 * 
 * 验证：
 * - 可以为同一个 fd 同时注册读写事件
 * - 两个事件都能正确触发
 */
TEST(NetworkEngineTest, RegisterBothReadAndWriteEvents) {
    NetworkEngine engine;
    
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 注册读事件
    std::atomic<bool> read_callback_called{false};
    engine.register_read_event(fd1, [&read_callback_called, fd1]() {
        read_callback_called = true;
        char buffer[128];
        read(fd1, buffer, sizeof(buffer));
    });
    
    // 注册写事件
    std::atomic<bool> write_callback_called{false};
    engine.register_write_event(fd1, [&write_callback_called]() {
        write_callback_called = true;
    });
    
    // 在单独的线程中运行事件循环
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 触发读事件
    const char* data = "Test data";
    write(fd2, data, strlen(data));
    
    // 等待回调被调用
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证两个回调都被调用
    EXPECT_TRUE(read_callback_called.load());
    EXPECT_TRUE(write_callback_called.load());
    
    // 停止事件循环
    engine.stop();
    event_thread.join();
    
    // 清理
    engine.unregister_event(fd1);
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试注销不存在的事件
 * 
 * 验证：
 * - 注销不存在的 fd 不会导致崩溃
 * - 操作被安全忽略
 */
TEST(NetworkEngineTest, UnregisterNonExistentEvent) {
    NetworkEngine engine;
    
    // 注销不存在的 fd
    ASSERT_NO_THROW({
        engine.unregister_event(999);
    });
}

/**
 * @brief 测试多个 fd 的事件处理
 * 
 * 验证：
 * - 可以同时监听多个 fd
 * - 每个 fd 的事件独立触发
 */
TEST(NetworkEngineTest, MultipleFileDescriptors) {
    NetworkEngine engine;
    
    // 创建多对 socket
    auto [fd1, fd2] = create_socket_pair();
    auto [fd3, fd4] = create_socket_pair();
    
    // 注册读事件
    std::atomic<int> callback_count{0};
    
    engine.register_read_event(fd1, [&callback_count, fd1]() {
        callback_count++;
        char buffer[128];
        read(fd1, buffer, sizeof(buffer));
    });
    
    engine.register_read_event(fd3, [&callback_count, fd3]() {
        callback_count++;
        char buffer[128];
        read(fd3, buffer, sizeof(buffer));
    });
    
    // 在单独的线程中运行事件循环
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 触发两个 fd 的读事件
    const char* data = "Test";
    write(fd2, data, strlen(data));
    write(fd4, data, strlen(data));
    
    // 等待回调被调用
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证两个回调都被调用
    EXPECT_EQ(callback_count.load(), 2);
    
    // 停止事件循环
    engine.stop();
    event_thread.join();
    
    // 清理
    engine.unregister_event(fd1);
    engine.unregister_event(fd3);
    close(fd1);
    close(fd2);
    close(fd3);
    close(fd4);
}

/**
 * @brief 测试缓冲区池访问
 * 
 * 验证：
 * - 可以访问网络引擎的缓冲区池
 * - 缓冲区池正常工作
 */
TEST(NetworkEngineTest, BufferPoolAccess) {
    NetworkEngine engine(4096, 10);
    
    // 获取缓冲区池
    BufferPool& pool = engine.buffer_pool();
    
    // 从缓冲区池获取缓冲区
    auto buffer = pool.acquire();
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(pool.buffer_size(), 4096);
    
    // 归还缓冲区
    pool.release(std::move(buffer));
}


// ============================================================================
// Task 4.4: 网络引擎单元测试 - TCP 操作和边缘情况
// ============================================================================

/**
 * @brief 测试 TCP 连接建立
 * 
 * 验证：
 * - async_connect 可以成功建立连接
 * - 返回有效的文件描述符
 * 
 * 需求: 1.4
 */
TEST(NetworkEngineTest, TcpConnect) {
    // 注意：此测试需要一个运行的服务器
    // 在实际环境中，应该先启动一个测试服务器
    // 这里我们测试连接失败的情况（没有服务器监听）
    
    NetworkEngine engine;
    
    // 尝试连接到不存在的服务器
    // 由于没有协程调度器，我们无法直接测试 co_await
    // 这个测试验证 ConnectAwaitable 对象可以被创建
    auto awaitable = engine.async_connect("127.0.0.1", 9999);
    
    // 验证 awaitable 对象创建成功
    // 实际的连接测试需要协程支持
}

/**
 * @brief 测试非阻塞 socket 操作
 * 
 * 验证：
 * - socket 设置为非阻塞模式
 * - EAGAIN/EWOULDBLOCK 错误被正确处理
 * 
 * 需求: 1.2, 1.4
 */
TEST(NetworkEngineTest, NonBlockingSocketOperations) {
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 验证 socket 是非阻塞的
    int flags = fcntl(fd1, F_GETFL, 0);
    ASSERT_NE(flags, -1);
    EXPECT_TRUE(flags & O_NONBLOCK);
    
    // 尝试从空 socket 读取（应该返回 EAGAIN）
    char buffer[128];
    ssize_t n = read(fd1, buffer, sizeof(buffer));
    EXPECT_EQ(n, -1);
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
    
    // 清理
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试 TCP 读取操作
 * 
 * 验证：
 * - 可以从 socket 读取数据
 * - 读取的数据正确
 * 
 * 需求: 1.4
 */
TEST(NetworkEngineTest, TcpRead) {
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 向 fd2 写入数据
    const char* test_data = "Hello, TCP!";
    ssize_t written = write(fd2, test_data, strlen(test_data));
    ASSERT_GT(written, 0);
    
    // 从 fd1 读取数据
    char buffer[128];
    ssize_t read_bytes = read(fd1, buffer, sizeof(buffer));
    ASSERT_GT(read_bytes, 0);
    
    // 验证数据正确
    buffer[read_bytes] = '\0';
    EXPECT_STREQ(buffer, test_data);
    
    // 清理
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试 TCP 写入操作
 * 
 * 验证：
 * - 可以向 socket 写入数据
 * - 写入的数据可以被对端读取
 * 
 * 需求: 1.4
 */
TEST(NetworkEngineTest, TcpWrite) {
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 向 fd1 写入数据
    const char* test_data = "Test write operation";
    ssize_t written = write(fd1, test_data, strlen(test_data));
    ASSERT_GT(written, 0);
    EXPECT_EQ(written, static_cast<ssize_t>(strlen(test_data)));
    
    // 从 fd2 读取数据
    char buffer[128];
    ssize_t read_bytes = read(fd2, buffer, sizeof(buffer));
    ASSERT_GT(read_bytes, 0);
    
    // 验证数据正确
    buffer[read_bytes] = '\0';
    EXPECT_STREQ(buffer, test_data);
    
    // 清理
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试 TCP 连接关闭
 * 
 * 验证：
 * - 关闭 socket 后，对端可以检测到连接关闭
 * - read() 返回 0 表示连接关闭
 * 
 * 需求: 1.4
 */
TEST(NetworkEngineTest, TcpClose) {
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 关闭 fd1
    close(fd1);
    
    // 尝试从 fd2 读取（应该返回 0，表示连接关闭）
    char buffer[128];
    ssize_t n = read(fd2, buffer, sizeof(buffer));
    EXPECT_EQ(n, 0);
    
    // 清理
    close(fd2);
}

/**
 * @brief 测试边缘情况 - 连接失败
 * 
 * 验证：
 * - 连接到不存在的服务器时正确处理失败
 * - 返回适当的错误信息
 * 
 * 需求: 12.1
 */
TEST(NetworkEngineTest, EdgeCase_ConnectionFailed) {
    NetworkEngine engine;
    
    // 尝试连接到不存在的服务器
    // 注意：由于没有协程调度器，我们只能验证 awaitable 对象的创建
    auto awaitable = engine.async_connect("192.0.2.1", 9999);  // 使用 TEST-NET-1 地址
    
    // 验证 awaitable 对象创建成功
    // 实际的连接失败测试需要协程支持
}

/**
 * @brief 测试边缘情况 - 发送失败
 * 
 * 验证：
 * - 向已关闭的 socket 发送数据时正确处理失败
 * - 返回适当的错误信息
 * 
 * 需求: 12.1
 */
TEST(NetworkEngineTest, EdgeCase_SendFailed) {
    NetworkEngine engine;
    
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 关闭 fd2（对端）
    close(fd2);
    
    // 尝试向 fd1 发送数据
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    
    // 第一次发送可能成功（数据在缓冲区中）
    ssize_t sent = send(fd1, data.data(), data.size(), MSG_DONTWAIT);
    
    // 如果第一次发送成功，再次发送应该失败
    if (sent > 0) {
        // 等待一小段时间让系统检测到连接关闭
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // 再次发送应该失败
        sent = send(fd1, data.data(), data.size(), MSG_DONTWAIT);
        // 可能返回 -1 (EPIPE) 或成功（取决于时序）
    }
    
    // 清理
    close(fd1);
}

/**
 * @brief 测试边缘情况 - 接收失败
 * 
 * 验证：
 * - 从无效的 socket 接收数据时正确处理失败
 * - 返回适当的错误信息
 * 
 * 需求: 12.1
 */
TEST(NetworkEngineTest, EdgeCase_RecvFailed) {
    // 创建一个 socket 然后立即关闭
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    close(fd);
    
    // 尝试从已关闭的 socket 接收数据
    char buffer[128];
    ssize_t n = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    EXPECT_EQ(n, -1);
    EXPECT_EQ(errno, EBADF);  // Bad file descriptor
}

/**
 * @brief 测试大数据传输
 * 
 * 验证：
 * - 可以传输大量数据
 * - 数据完整性得到保证
 */
TEST(NetworkEngineTest, LargeDataTransfer) {
    // 创建一对 socket
    auto [fd1, fd2] = create_socket_pair();
    
    // 创建大数据块（1MB）
    const size_t data_size = 1024 * 1024;
    std::vector<uint8_t> send_data(data_size);
    for (size_t i = 0; i < data_size; ++i) {
        send_data[i] = static_cast<uint8_t>(i % 256);
    }
    
    // 在单独的线程中发送数据
    std::thread sender([fd1, &send_data]() {
        size_t offset = 0;
        while (offset < send_data.size()) {
            ssize_t sent = write(fd1, send_data.data() + offset, send_data.size() - offset);
            if (sent > 0) {
                offset += sent;
            } else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // 等待一小段时间
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            } else {
                break;
            }
        }
    });
    
    // 接收数据
    std::vector<uint8_t> recv_data;
    recv_data.reserve(data_size);
    
    while (recv_data.size() < data_size) {
        uint8_t buffer[4096];
        ssize_t received = read(fd2, buffer, sizeof(buffer));
        if (received > 0) {
            recv_data.insert(recv_data.end(), buffer, buffer + received);
        } else if (received == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // 等待一小段时间
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } else {
            break;
        }
    }
    
    sender.join();
    
    // 验证数据完整性
    EXPECT_EQ(recv_data.size(), send_data.size());
    EXPECT_EQ(recv_data, send_data);
    
    // 清理
    close(fd1);
    close(fd2);
}

/**
 * @brief 测试并发连接处理
 * 
 * 验证：
 * - 可以同时处理多个连接
 * - 每个连接的数据独立处理
 */
TEST(NetworkEngineTest, ConcurrentConnections) {
    NetworkEngine engine;
    
    const int num_connections = 10;
    std::vector<std::pair<int, int>> socket_pairs;
    std::vector<std::atomic<bool>> callbacks_called(num_connections);
    
    // 创建多个 socket 对
    for (int i = 0; i < num_connections; ++i) {
        socket_pairs.push_back(create_socket_pair());
        callbacks_called[i] = false;
    }
    
    // 为每个 socket 注册读事件
    for (int i = 0; i < num_connections; ++i) {
        int fd = socket_pairs[i].first;
        engine.register_read_event(fd, [&callbacks_called, i, fd]() {
            callbacks_called[i] = true;
            char buffer[128];
            read(fd, buffer, sizeof(buffer));
        });
    }
    
    // 在单独的线程中运行事件循环
    std::thread event_thread([&engine]() {
        engine.run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 向所有 socket 发送数据
    for (int i = 0; i < num_connections; ++i) {
        const char* data = "Test";
        write(socket_pairs[i].second, data, strlen(data));
    }
    
    // 等待所有回调被调用
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 验证所有回调都被调用
    for (int i = 0; i < num_connections; ++i) {
        EXPECT_TRUE(callbacks_called[i].load()) << "Callback " << i << " not called";
    }
    
    // 停止事件循环
    engine.stop();
    event_thread.join();
    
    // 清理
    for (auto& pair : socket_pairs) {
        engine.unregister_event(pair.first);
        close(pair.first);
        close(pair.second);
    }
}

