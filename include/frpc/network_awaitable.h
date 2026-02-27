#pragma once

#include <coroutine>
#include <cstdint>
#include <span>
#include <string>

#include "network_engine.h"

namespace frpc {

// Forward declaration
class NetworkEngine;

/**
 * @brief 异步发送操作的 Awaitable 对象
 * 
 * 封装异步发送操作，使其可以通过 co_await 使用。
 * 当操作未完成时挂起协程，操作完成后恢复协程。
 * 
 * @example
 * ```cpp
 * std::vector<uint8_t> data = {1, 2, 3, 4, 5};
 * auto result = co_await engine.async_send(fd, data);
 * if (result.success) {
 *     std::cout << "Sent " << result.bytes_sent << " bytes" << std::endl;
 * }
 * ```
 */
class SendAwaitable {
public:
    /**
     * @brief 构造函数
     * @param engine 网络引擎指针
     * @param fd 文件描述符
     * @param data 要发送的数据
     */
    SendAwaitable(NetworkEngine* engine, int fd, std::span<const uint8_t> data);
    
    /**
     * @brief 检查操作是否已完成
     * @return true 表示操作已完成，无需挂起
     * 
     * 如果数据可以立即发送（socket 缓冲区有空间），则返回 true。
     * 否则返回 false，协程将被挂起。
     */
    bool await_ready() const noexcept;
    
    /**
     * @brief 挂起协程
     * @param handle 当前协程句柄
     * 
     * 将协程句柄保存，以便操作完成后恢复。
     * 注册网络事件回调，当 socket 可写时恢复协程。
     */
    void await_suspend(std::coroutine_handle<> handle);
    
    /**
     * @brief 获取操作结果
     * @return 发送结果
     * 
     * 当协程恢复时调用，返回发送操作的结果。
     */
    SendResult await_resume() const;

private:
    NetworkEngine* engine_;                 ///< 网络引擎指针
    int fd_;                                ///< 文件描述符
    std::span<const uint8_t> data_;         ///< 要发送的数据
    bool completed_;                        ///< 操作是否完成
    std::coroutine_handle<> handle_;        ///< 协程句柄
    SendResult result_;                     ///< 发送结果
    
    /**
     * @brief 尝试立即发送数据
     * @return true 如果发送完成
     */
    bool try_send();
    
    /**
     * @brief 写事件回调
     */
    void on_write_ready();
};

/**
 * @brief 异步接收操作的 Awaitable 对象
 * 
 * 封装异步接收操作，使其可以通过 co_await 使用。
 * 当没有数据可读时挂起协程，有数据时恢复协程。
 * 
 * @example
 * ```cpp
 * std::vector<uint8_t> buffer(1024);
 * ssize_t bytes = co_await engine.async_recv(fd, buffer);
 * if (bytes > 0) {
 *     // 处理接收到的数据
 * }
 * ```
 */
class RecvAwaitable {
public:
    /**
     * @brief 构造函数
     * @param engine 网络引擎指针
     * @param fd 文件描述符
     * @param buffer 接收缓冲区
     */
    RecvAwaitable(NetworkEngine* engine, int fd, std::span<uint8_t> buffer);
    
    /**
     * @brief 检查操作是否已完成
     * @return true 表示有数据可读，无需挂起
     */
    bool await_ready() const noexcept;
    
    /**
     * @brief 挂起协程
     * @param handle 当前协程句柄
     * 
     * 注册读事件回调，当有数据可读时恢复协程。
     */
    void await_suspend(std::coroutine_handle<> handle);
    
    /**
     * @brief 获取操作结果
     * @return 实际接收的字节数（0 表示连接关闭，< 0 表示错误）
     */
    ssize_t await_resume() const;

private:
    NetworkEngine* engine_;                 ///< 网络引擎指针
    int fd_;                                ///< 文件描述符
    std::span<uint8_t> buffer_;             ///< 接收缓冲区
    bool completed_;                        ///< 操作是否完成
    std::coroutine_handle<> handle_;        ///< 协程句柄
    ssize_t bytes_received_;                ///< 接收的字节数
    
    /**
     * @brief 尝试立即接收数据
     * @return true 如果接收完成
     */
    bool try_recv();
    
    /**
     * @brief 读事件回调
     */
    void on_read_ready();
};

/**
 * @brief 异步连接操作的 Awaitable 对象
 * 
 * 封装异步连接操作，使其可以通过 co_await 使用。
 * 当连接未建立时挂起协程，连接建立后恢复协程。
 * 
 * @example
 * ```cpp
 * int fd = co_await engine.async_connect("127.0.0.1", 8080);
 * // 使用连接...
 * ```
 */
class ConnectAwaitable {
public:
    /**
     * @brief 构造函数
     * @param engine 网络引擎指针
     * @param addr 远程地址
     * @param port 远程端口
     */
    ConnectAwaitable(NetworkEngine* engine, const std::string& addr, uint16_t port);
    
    /**
     * @brief 检查操作是否已完成
     * @return true 表示连接已建立，无需挂起
     */
    bool await_ready() const noexcept;
    
    /**
     * @brief 挂起协程
     * @param handle 当前协程句柄
     * 
     * 发起非阻塞连接，注册写事件回调（连接建立时 socket 变为可写）。
     */
    void await_suspend(std::coroutine_handle<> handle);
    
    /**
     * @brief 获取操作结果
     * @return 连接的文件描述符
     * @throws NetworkException 如果连接失败
     */
    int await_resume() const;

private:
    NetworkEngine* engine_;                 ///< 网络引擎指针
    std::string addr_;                      ///< 远程地址
    uint16_t port_;                         ///< 远程端口
    bool completed_;                        ///< 操作是否完成
    std::coroutine_handle<> handle_;        ///< 协程句柄
    int fd_;                                ///< 连接的文件描述符
    int error_code_;                        ///< 错误码
    
    /**
     * @brief 尝试立即连接
     * @return true 如果连接完成
     */
    bool try_connect();
    
    /**
     * @brief 写事件回调（连接建立）
     */
    void on_connect_ready();
};

}  // namespace frpc

