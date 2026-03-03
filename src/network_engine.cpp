/**
 * @file network_engine.cpp
 * @brief Reactor 模式的网络引擎实现
 * 
 * ============================================================================
 * Reactor 模式通俗解释
 * ============================================================================
 * 
 * 想象一个餐厅的运作方式：
 * 
 * 传统方式（多线程阻塞 I/O）：
 * - 每个顾客（连接）配一个服务员（线程）
 * - 服务员一直守在桌边等顾客点餐（阻塞等待）
 * - 1000 个顾客需要 1000 个服务员 → 成本高，效率低
 * 
 * Reactor 方式（事件驱动非阻塞 I/O）：
 * - 只需要少数几个服务员（线程）
 * - 顾客需要服务时按铃（事件通知）
 * - 服务员巡视所有桌子，哪个按铃就服务哪个
 * - 1000 个顾客可能只需要几个服务员 → 高效！
 * 
 * ============================================================================
 * Reactor 模式的三大组件
 * ============================================================================
 * 
 * 1. 事件多路复用器（Event Demultiplexer）- epoll
 *    - 作用：同时监听多个 socket，等待事件发生
 *    - 实现：Linux 的 epoll 系统调用
 *    - 类比：餐厅的"呼叫系统"，能同时监听所有桌子的按铃
 * 
 * 2. 事件分发器（Event Dispatcher）- run() 方法
 *    - 作用：检测到事件后，分发给对应的处理器
 *    - 实现：事件循环 + epoll_wait
 *    - 类比：服务员听到按铃后，走到对应的桌子
 * 
 * 3. 事件处理器（Event Handler）- EventHandler 结构体
 *    - 作用：处理具体的业务逻辑（读数据、写数据、处理错误）
 *    - 实现：回调函数（on_read, on_write, on_error）
 *    - 类比：服务员到达桌子后的具体服务动作（点餐、上菜、结账）
 * 
 * ============================================================================
 * 工作流程示例
 * ============================================================================
 * 
 * 1. 初始化阶段：
 *    NetworkEngine engine;  // 创建 epoll，初始化"监控中心"
 * 
 * 2. 注册阶段：
 *    engine.register_read_event(fd, callback);  // 告诉 epoll："监听这个 socket"
 * 
 * 3. 运行阶段：
 *    engine.run();  // 启动事件循环
 *    ↓
 *    epoll_wait() 阻塞等待  // 等待任何 socket 有事件
 *    ↓
 *    检测到事件（比如 socket 5 有数据到达）
 *    ↓
 *    查找 handlers_[5]，找到对应的回调函数
 *    ↓
 *    调用 callback()  // 执行业务逻辑
 *    ↓
 *    继续 epoll_wait()  // 等待下一个事件
 * 
 * ============================================================================
 * 边缘触发（EPOLLET）vs 水平触发
 * ============================================================================
 * 
 * 边缘触发（本实现采用）：
 * - 只在状态变化时通知一次
 * - 例子：门铃只响一次，你必须把所有快递都拿进来
 * - 优点：通知次数少，性能高
 * - 缺点：必须一次性处理完所有数据，编程复杂
 * 
 * 水平触发（默认模式）：
 * - 只要有数据就持续通知
 * - 例子：门铃一直响，直到你把快递拿完
 * - 优点：不会丢数据，编程简单
 * - 缺点：可能产生大量重复通知
 * 
 * ============================================================================
 */

#include "frpc/network_engine.h"
#include "frpc/network_awaitable.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include "frpc/logger.h"

namespace frpc {

NetworkEngine::NetworkEngine(size_t buffer_size, size_t initial_buffers)
    : epoll_fd_(-1)
    , running_(false)
    , buffer_pool_(buffer_size, initial_buffers) {
    
    // ============ Reactor 模式初始化 ============
    // 创建 epoll 实例，这是 Reactor 模式的核心：事件多路复用器
    // 
    // 类比：epoll 就像一个"监控中心"，可以同时监视成千上万个 socket
    // 当任何一个 socket 有数据到达或可以写入时，epoll 会立即通知我们
    // 
    // 相比传统的 select/poll，epoll 的优势：
    // - 没有文件描述符数量限制（select 最多 1024 个）
    // - 性能不会随监控数量增加而下降（O(1) vs O(n)）
    // - 支持边缘触发模式，减少不必要的事件通知
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        throw NetworkException(
            ErrorCode::NetworkError,
            "Failed to create epoll: " + std::string(std::strerror(errno))
        );
    }
    
    Logger::instance().info("NetworkEngine initialized with epoll_fd={}", epoll_fd_);
}

NetworkEngine::~NetworkEngine() {
    stop();
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        Logger::instance().info("NetworkEngine destroyed, epoll_fd={} closed", epoll_fd_);
    }
}

void NetworkEngine::run() {
    if (running_.exchange(true)) {
        Logger::instance().warn("NetworkEngine::run() called but already running");
        return;
    }
    
    Logger::instance().info("NetworkEngine event loop started");
    
    // ============ Reactor 模式的核心：事件循环 ============
    // 
    // 这是 Reactor 模式的"心脏"，不断循环执行以下步骤：
    // 1. 等待事件发生（epoll_wait）
    // 2. 检测到事件后，分发给对应的处理器
    // 3. 处理器执行回调函数
    // 4. 继续等待下一批事件
    //
    // 类比：就像一个餐厅服务员，不断巡视各个桌子（socket）
    // 哪个桌子有需求（事件），就去处理哪个桌子
    
    const int MAX_EVENTS = 64;  // 每次最多处理 64 个事件（批量处理提高效率）
    struct epoll_event events[MAX_EVENTS];
    
    while (running_.load()) {
        // -------- 步骤 1: 等待事件 --------
        // epoll_wait 是一个阻塞调用，会等待直到：
        // - 有 socket 事件发生（数据到达、可写、错误等）
        // - 超时（100ms）- 这样可以及时响应 stop() 请求
        //
        // 类比：服务员站在餐厅中央，等待顾客招手
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                // 被信号中断（比如 Ctrl+C），这是正常情况，继续循环
                continue;
            }
            
            Logger::instance().error("epoll_wait failed: {}", std::strerror(errno));
            throw NetworkException(
                ErrorCode::NetworkError,
                "epoll_wait failed: " + std::string(std::strerror(errno))
            );
        }
        
        // -------- 步骤 2 & 3: 事件分发和处理 --------
        // nfds 表示有多少个 socket 发生了事件
        // 遍历这些事件，根据事件类型调用对应的回调函数
        //
        // 类比：服务员依次处理每个招手的顾客
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;           // 哪个 socket 发生了事件
            uint32_t event_mask = events[i].events;  // 发生了什么类型的事件
            
            // 查找这个 socket 对应的事件处理器
            auto it = handlers_.find(fd);
            if (it == handlers_.end()) {
                // 找不到处理器，可能是已经被注销了
                Logger::instance().warn("Received event for unregistered fd={}", fd);
                continue;
            }
            
            const EventHandler& handler = it->second;
            
            // -------- 处理不同类型的事件 --------
            
            // 1. 错误事件（连接断开、socket 错误等）
            // EPOLLERR: socket 发生错误
            // EPOLLHUP: 对端关闭连接（挂断）
            if (event_mask & (EPOLLERR | EPOLLHUP)) {
                Logger::instance().debug("Error event on fd={}", fd);
                if (handler.on_error) {
                    handler.on_error();  // 调用错误处理回调
                }
                continue;  // 错误事件优先处理，跳过读写事件
            }
            
            // 2. 读事件（有数据可读）
            // EPOLLIN: socket 接收缓冲区有数据，可以读取
            // 类比：顾客点餐（客户端发送数据过来）
            if (event_mask & EPOLLIN) {
                Logger::instance().debug("Read event on fd={}", fd);
                if (handler.on_read) {
                    handler.on_read();  // 调用读事件回调
                }
            }
            
            // 3. 写事件（可以写入数据）
            // EPOLLOUT: socket 发送缓冲区有空间，可以写入
            // 类比：厨房准备好了，可以上菜（服务端发送数据给客户端）
            if (event_mask & EPOLLOUT) {
                Logger::instance().debug("Write event on fd={}", fd);
                if (handler.on_write) {
                    handler.on_write();  // 调用写事件回调
                }
            }
        }
    }
    
    Logger::instance().info("NetworkEngine event loop stopped");
}

void NetworkEngine::stop() {
    if (running_.exchange(false)) {
        Logger::instance().info("NetworkEngine stop requested");
    }
}

bool NetworkEngine::register_read_event(int fd, EventCallback callback) {
    // ============ 注册读事件：告诉 Reactor 监听这个 socket 的数据到达 ============
    //
    // 工作流程：
    // 1. 保存回调函数到 handlers_ 映射表
    // 2. 通过 epoll_ctl 告诉内核："请监听这个 socket 的读事件"
    // 3. 当数据到达时，事件循环会自动调用我们的回调函数
    //
    // 类比：在餐厅前台登记"当 X 号桌有顾客时，请通知我"
    
    auto it = handlers_.find(fd);
    
    if (it == handlers_.end()) {
        // -------- 情况 1: 首次注册这个 socket --------
        EventHandler handler;
        handler.on_read = std::move(callback);  // 保存读事件回调函数
        handlers_[fd] = std::move(handler);
        
        // 配置 epoll 事件
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;  // EPOLLIN=读事件, EPOLLET=边缘触发模式
        ev.data.fd = fd;                // 关联文件描述符
        
        // -------- 边缘触发 vs 水平触发 --------
        // 边缘触发（EPOLLET）：只在状态变化时通知一次
        //   - 优点：减少事件通知次数，性能更高
        //   - 要求：必须一次性读完所有数据，否则会丢失后续通知
        //   - 例子：门铃只响一次，你必须把所有快递都拿进来
        //
        // 水平触发（默认）：只要有数据就持续通知
        //   - 优点：不会丢失数据，编程简单
        //   - 缺点：可能产生大量重复通知，性能较低
        //   - 例子：门铃一直响，直到你把快递拿完
        
        // 将 socket 添加到 epoll 监控列表
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            Logger::instance().error("Failed to add fd={} to epoll: {}", fd, std::strerror(errno));
            handlers_.erase(fd);  // 失败了，清理已保存的回调
            return false;
        }
        
        Logger::instance().debug("Registered read event for fd={}", fd);
    } else {
        // -------- 情况 2: 更新已存在的 socket --------
        // 这个 socket 之前可能注册过写事件，现在要添加/更新读事件
        it->second.on_read = std::move(callback);
        
        // 修改 epoll 监听的事件类型
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;  // 读事件
        if (it->second.on_write) {
            ev.events |= EPOLLOUT;  // 如果之前注册了写事件，保留它
        }
        ev.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
            Logger::instance().error("Failed to modify fd={} in epoll: {}", fd, std::strerror(errno));
            return false;
        }
        
        Logger::instance().debug("Updated read event for fd={}", fd);
    }
    
    return true;
}

bool NetworkEngine::register_write_event(int fd, EventCallback callback) {
    // ============ 注册写事件：告诉 Reactor 监听这个 socket 何时可以写入 ============
    //
    // 为什么需要写事件？
    // - socket 发送缓冲区可能已满，直接 write() 会阻塞或返回 EAGAIN
    // - 注册写事件后，当缓冲区有空间时，epoll 会通知我们
    // - 这样就可以实现非阻塞的数据发送
    //
    // 类比：厨房忙不过来时，服务员等待厨房通知"可以接新订单了"
    
    auto it = handlers_.find(fd);
    
    if (it == handlers_.end()) {
        // -------- 情况 1: 首次注册这个 socket --------
        EventHandler handler;
        handler.on_write = std::move(callback);  // 保存写事件回调函数
        handlers_[fd] = std::move(handler);
        
        // 配置 epoll 事件
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;  // EPOLLOUT=写事件, EPOLLET=边缘触发
        ev.data.fd = fd;
        
        // 将 socket 添加到 epoll 监控列表
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            Logger::instance().error("Failed to add fd={} to epoll: {}", fd, std::strerror(errno));
            handlers_.erase(fd);
            return false;
        }
        
        Logger::instance().debug("Registered write event for fd={}", fd);
    } else {
        // -------- 情况 2: 更新已存在的 socket --------
        it->second.on_write = std::move(callback);
        
        // 修改 epoll 监听的事件类型
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;  // 写事件
        if (it->second.on_read) {
            ev.events |= EPOLLIN;  // 如果之前注册了读事件，保留它
        }
        ev.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
            Logger::instance().error("Failed to modify fd={} in epoll: {}", fd, std::strerror(errno));
            return false;
        }
        
        Logger::instance().debug("Updated write event for fd={}", fd);
    }
    
    return true;
}

void NetworkEngine::unregister_event(int fd) {
    // ============ 注销事件：停止监听这个 socket ============
    //
    // 使用场景：
    // - 连接关闭时，不再需要监听这个 socket
    // - 清理资源，避免内存泄漏
    //
    // 类比：顾客离开餐厅，取消对这个桌子的关注
    
    auto it = handlers_.find(fd);
    if (it == handlers_.end()) {
        // 这个 socket 没有注册过，可能已经被注销了
        Logger::instance().warn("Attempted to unregister non-existent fd={}", fd);
        return;
    }
    
    // 从 epoll 监控列表中移除这个 socket
    // 之后 epoll_wait 不会再返回这个 socket 的任何事件
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        Logger::instance().error("Failed to remove fd={} from epoll: {}", fd, std::strerror(errno));
    }
    
    // 从处理器映射中移除，释放回调函数占用的内存
    handlers_.erase(it);
    
    Logger::instance().debug("Unregistered events for fd={}", fd);
}

SendAwaitable NetworkEngine::async_send(int fd, std::span<const uint8_t> data) {
    return SendAwaitable(this, fd, data);
}

RecvAwaitable NetworkEngine::async_recv(int fd, std::span<uint8_t> buffer) {
    return RecvAwaitable(this, fd, buffer);
}

ConnectAwaitable NetworkEngine::async_connect(const std::string& addr, uint16_t port) {
    return ConnectAwaitable(this, addr, port);
}

}  // namespace frpc

