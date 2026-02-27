#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "buffer_pool.h"
#include "exceptions.h"

namespace frpc {

// Forward declarations for Awaitable classes
// Full definitions are in network_awaitable.h
class SendAwaitable;
class RecvAwaitable;
class ConnectAwaitable;

/**
 * @brief 事件回调函数类型
 * 
 * 当注册的事件触发时，网络引擎会调用此回调函数。
 * 回调函数应该尽快返回，避免阻塞事件循环。
 */
using EventCallback = std::function<void()>;

/**
 * @brief 发送操作结果
 */
struct SendResult {
    bool success;           ///< 发送是否成功
    ssize_t bytes_sent;     ///< 实际发送的字节数
    int error_code;         ///< 错误码（成功时为 0）
};

/**
 * @brief 事件处理器
 * 
 * 存储与文件描述符关联的事件回调函数。
 * 当对应的事件触发时，网络引擎会调用相应的回调。
 */
struct EventHandler {
    EventCallback on_read;   ///< 读事件回调
    EventCallback on_write;  ///< 写事件回调
    EventCallback on_error;  ///< 错误事件回调
};

/**
 * @brief 网络引擎 - 基于 Reactor 模式的事件驱动网络层
 * 
 * NetworkEngine 实现了高性能的非阻塞网络 I/O，使用 epoll 进行事件多路复用。
 * 采用边缘触发（EPOLLET）模式以提高性能，所有 socket 操作都是非阻塞的。
 * 
 * ## Reactor 模型
 * 
 * Reactor 模式是一种事件驱动的设计模式，用于处理并发 I/O 操作：
 * 
 * 1. **事件多路复用器（Event Demultiplexer）**: 使用 epoll 监听多个文件描述符
 * 2. **事件分发器（Event Dispatcher）**: 事件循环检测到事件后分发给对应的处理器
 * 3. **事件处理器（Event Handler）**: 处理具体的 I/O 操作（读、写、错误）
 * 
 * ## 工作流程
 * 
 * ```
 * ┌─────────────┐
 * │ Application │
 * └──────┬──────┘
 *        │ register_event()
 *        ▼
 * ┌─────────────────┐
 * │ NetworkEngine   │
 * │  ┌───────────┐  │
 * │  │Event Loop │◄─┼─── run()
 * │  └─────┬─────┘  │
 * │        │        │
 * │        ▼        │
 * │  ┌───────────┐  │
 * │  │   epoll   │  │
 * │  └─────┬─────┘  │
 * │        │        │
 * │        ▼        │
 * │  ┌───────────┐  │
 * │  │ Handlers  │  │
 * │  └─────┬─────┘  │
 * └────────┼────────┘
 *          │ callback()
 *          ▼
 * ┌─────────────┐
 * │ Application │
 * └─────────────┘
 * ```
 * 
 * ## 边缘触发模式（EPOLLET）
 * 
 * 使用边缘触发模式而非水平触发，优势：
 * - 减少事件通知次数，提高性能
 * - 避免重复处理相同事件
 * - 更适合高并发场景
 * 
 * 注意事项：
 * - 必须一次性读取/写入所有可用数据
 * - 需要处理 EAGAIN/EWOULDBLOCK 错误
 * - socket 必须设置为非阻塞模式
 * 
 * ## 协程集成
 * 
 * NetworkEngine 提供协程友好的异步接口，通过 Awaitable 对象支持 co_await 语法：
 * 
 * ```cpp
 * // 异步连接
 * int fd = co_await engine.async_connect("127.0.0.1", 8080);
 * 
 * // 异步发送
 * auto result = co_await engine.async_send(fd, data);
 * 
 * // 异步接收
 * ssize_t bytes = co_await engine.async_recv(fd, buffer);
 * ```
 * 
 * ## 缓冲区管理
 * 
 * 集成 BufferPool 进行高效的缓冲区管理：
 * - 预分配缓冲区，避免频繁内存分配
 * - 自动归还缓冲区到池中
 * - 减少内存碎片
 * 
 * @note 线程安全：当前实现为单线程事件循环，不支持多线程并发访问
 * @note 性能：边缘触发模式 + 非阻塞 I/O + 缓冲区池 = 高性能
 * 
 * @example 基本使用
 * ```cpp
 * NetworkEngine engine;
 * 
 * // 注册读事件
 * engine.register_read_event(fd, []() {
 *     std::cout << "Data available to read" << std::endl;
 * });
 * 
 * // 启动事件循环
 * engine.run();
 * ```
 * 
 * @example 协程使用
 * ```cpp
 * RpcTask<void> handle_connection(NetworkEngine& engine) {
 *     int fd = co_await engine.async_connect("127.0.0.1", 8080);
 *     
 *     std::vector<uint8_t> data = {1, 2, 3, 4, 5};
 *     auto result = co_await engine.async_send(fd, data);
 *     
 *     if (result.success) {
 *         std::vector<uint8_t> buffer(1024);
 *         ssize_t bytes = co_await engine.async_recv(fd, buffer);
 *         // 处理接收到的数据...
 *     }
 * }
 * ```
 */
class NetworkEngine {
public:
    /**
     * @brief 构造函数
     * @param buffer_size 缓冲区大小（字节），默认 8KB
     * @param initial_buffers 初始缓冲区数量，默认 100
     * 
     * @throws NetworkException 如果 epoll 初始化失败
     * 
     * @example
     * // 使用默认参数
     * NetworkEngine engine;
     * 
     * // 自定义缓冲区配置
     * NetworkEngine engine(16384, 200);  // 16KB 缓冲区，200 个
     */
    NetworkEngine(size_t buffer_size = 8192, size_t initial_buffers = 100);
    
    /**
     * @brief 析构函数 - 清理资源
     * 
     * 自动停止事件循环并关闭 epoll 文件描述符。
     */
    ~NetworkEngine();
    
    // 禁止拷贝和移动
    NetworkEngine(const NetworkEngine&) = delete;
    NetworkEngine& operator=(const NetworkEngine&) = delete;
    NetworkEngine(NetworkEngine&&) = delete;
    NetworkEngine& operator=(NetworkEngine&&) = delete;
    
    /**
     * @brief 启动事件循环
     * 
     * 进入事件循环，持续监听和处理网络事件，直到调用 stop() 方法。
     * 此方法会阻塞当前线程。
     * 
     * 事件循环流程：
     * 1. 调用 epoll_wait() 等待事件
     * 2. 遍历触发的事件
     * 3. 根据事件类型调用对应的回调函数
     * 4. 重复步骤 1-3，直到 stop() 被调用
     * 
     * @throws NetworkException 如果事件循环运行失败
     * 
     * @note 此方法会阻塞，通常在单独的线程中调用
     * @note 调用 stop() 后，事件循环会在处理完当前事件后退出
     * 
     * @example
     * NetworkEngine engine;
     * 
     * // 在单独的线程中运行事件循环
     * std::thread event_thread([&engine]() {
     *     engine.run();
     * });
     * 
     * // 主线程继续执行其他任务...
     * 
     * // 停止事件循环
     * engine.stop();
     * event_thread.join();
     */
    void run();
    
    /**
     * @brief 停止事件循环
     * 
     * 设置停止标志，事件循环会在处理完当前事件后退出。
     * 此方法是线程安全的，可以从任何线程调用。
     * 
     * @note 线程安全
     * @note 非阻塞，立即返回
     * 
     * @example
     * engine.stop();  // 请求停止
     * // 事件循环会在处理完当前事件后退出
     */
    void stop();
    
    /**
     * @brief 注册 socket 读事件
     * 
     * 当 socket 上有数据可读时，会调用提供的回调函数。
     * 使用边缘触发模式（EPOLLET），回调函数应该一次性读取所有可用数据。
     * 
     * @param fd 文件描述符（必须是有效的 socket）
     * @param callback 事件触发时的回调函数
     * @return 注册是否成功
     * 
     * @note socket 必须设置为非阻塞模式
     * @note 边缘触发模式：回调函数应该循环读取直到 EAGAIN
     * 
     * @example
     * int fd = socket(AF_INET, SOCK_STREAM, 0);
     * set_nonblocking(fd);
     * 
     * engine.register_read_event(fd, [fd]() {
     *     char buffer[1024];
     *     while (true) {
     *         ssize_t n = read(fd, buffer, sizeof(buffer));
     *         if (n > 0) {
     *             // 处理数据...
     *         } else if (n == 0) {
     *             // 连接关闭
     *             break;
     *         } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
     *             // 没有更多数据
     *             break;
     *         } else {
     *             // 错误
     *             break;
     *         }
     *     }
     * });
     */
    bool register_read_event(int fd, EventCallback callback);
    
    /**
     * @brief 注册 socket 写事件
     * 
     * 当 socket 可写时，会调用提供的回调函数。
     * 使用边缘触发模式（EPOLLET），回调函数应该一次性写入所有待发送数据。
     * 
     * @param fd 文件描述符（必须是有效的 socket）
     * @param callback 事件触发时的回调函数
     * @return 注册是否成功
     * 
     * @note socket 必须设置为非阻塞模式
     * @note 边缘触发模式：回调函数应该循环写入直到 EAGAIN
     * 
     * @example
     * engine.register_write_event(fd, [fd, &data]() {
     *     size_t offset = 0;
     *     while (offset < data.size()) {
     *         ssize_t n = write(fd, data.data() + offset, data.size() - offset);
     *         if (n > 0) {
     *             offset += n;
     *         } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
     *             // socket 缓冲区满，等待下次写事件
     *             break;
     *         } else {
     *             // 错误
     *             break;
     *         }
     *     }
     * });
     */
    bool register_write_event(int fd, EventCallback callback);
    
    /**
     * @brief 注销事件
     * 
     * 从 epoll 中移除文件描述符，停止监听所有事件。
     * 
     * @param fd 文件描述符
     * 
     * @note 调用此方法后，不会再收到该 fd 的任何事件通知
     * @note 不会关闭文件描述符，调用者需要自行关闭
     * 
     * @example
     * engine.unregister_event(fd);
     * close(fd);  // 调用者负责关闭 fd
     */
    void unregister_event(int fd);
    
    /**
     * @brief 异步发送数据（协程友好）
     * 
     * 返回一个 Awaitable 对象，可以在协程中使用 co_await 等待发送完成。
     * 
     * @param fd 文件描述符
     * @param data 要发送的数据
     * @return SendAwaitable 对象，可用于 co_await
     * 
     * @note socket 必须设置为非阻塞模式
     * @note 此方法会尝试立即发送，如果无法立即完成则挂起协程
     * 
     * @example
     * std::vector<uint8_t> data = {1, 2, 3, 4, 5};
     * auto result = co_await engine.async_send(fd, data);
     * if (result.success) {
     *     std::cout << "Sent " << result.bytes_sent << " bytes" << std::endl;
     * } else {
     *     std::cerr << "Send failed: " << result.error_code << std::endl;
     * }
     */
    SendAwaitable async_send(int fd, std::span<const uint8_t> data);
    
    /**
     * @brief 异步接收数据（协程友好）
     * 
     * 返回一个 Awaitable 对象，可以在协程中使用 co_await 等待接收数据。
     * 
     * @param fd 文件描述符
     * @param buffer 接收缓冲区
     * @return RecvAwaitable 对象，返回实际接收的字节数
     * 
     * @note socket 必须设置为非阻塞模式
     * @note 返回 0 表示连接关闭，< 0 表示错误
     * 
     * @example
     * std::vector<uint8_t> buffer(1024);
     * ssize_t bytes = co_await engine.async_recv(fd, buffer);
     * if (bytes > 0) {
     *     std::cout << "Received " << bytes << " bytes" << std::endl;
     *     // 处理数据...
     * } else if (bytes == 0) {
     *     std::cout << "Connection closed" << std::endl;
     * } else {
     *     std::cerr << "Receive failed" << std::endl;
     * }
     */
    RecvAwaitable async_recv(int fd, std::span<uint8_t> buffer);
    
    /**
     * @brief 异步连接到远程地址（协程友好）
     * 
     * 返回一个 Awaitable 对象，可以在协程中使用 co_await 等待连接建立。
     * 
     * @param addr 远程地址（IP 地址或主机名）
     * @param port 远程端口
     * @return ConnectAwaitable 对象，返回连接的文件描述符
     * 
     * @throws NetworkException 如果连接失败
     * 
     * @example
     * try {
     *     int fd = co_await engine.async_connect("127.0.0.1", 8080);
     *     std::cout << "Connected, fd = " << fd << std::endl;
     *     // 使用连接...
     * } catch (const NetworkException& e) {
     *     std::cerr << "Connection failed: " << e.what() << std::endl;
     * }
     */
    ConnectAwaitable async_connect(const std::string& addr, uint16_t port);
    
    /**
     * @brief 获取缓冲区池
     * @return 缓冲区池引用
     * 
     * @note 供内部使用，外部代码通常不需要直接访问
     */
    BufferPool& buffer_pool() { return buffer_pool_; }

private:
    int epoll_fd_;                                      ///< epoll 文件描述符
    std::unordered_map<int, EventHandler> handlers_;    ///< fd -> 事件处理器映射
    std::atomic<bool> running_;                         ///< 运行状态标志
    BufferPool buffer_pool_;                            ///< 缓冲区池
};

}  // namespace frpc

