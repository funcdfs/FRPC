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
    
    // 创建 epoll 文件描述符
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
    
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    
    while (running_.load()) {
        // 等待事件，超时时间 100ms（允许及时响应 stop()）
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                // 被信号中断，继续循环
                continue;
            }
            
            Logger::instance().error("epoll_wait failed: {}", std::strerror(errno));
            throw NetworkException(
                ErrorCode::NetworkError,
                "epoll_wait failed: " + std::string(std::strerror(errno))
            );
        }
        
        // 处理触发的事件
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t event_mask = events[i].events;
            
            auto it = handlers_.find(fd);
            if (it == handlers_.end()) {
                Logger::instance().warn("Received event for unregistered fd={}", fd);
                continue;
            }
            
            const EventHandler& handler = it->second;
            
            // 处理错误事件
            if (event_mask & (EPOLLERR | EPOLLHUP)) {
                Logger::instance().debug("Error event on fd={}", fd);
                if (handler.on_error) {
                    handler.on_error();
                }
                continue;
            }
            
            // 处理读事件
            if (event_mask & EPOLLIN) {
                Logger::instance().debug("Read event on fd={}", fd);
                if (handler.on_read) {
                    handler.on_read();
                }
            }
            
            // 处理写事件
            if (event_mask & EPOLLOUT) {
                Logger::instance().debug("Write event on fd={}", fd);
                if (handler.on_write) {
                    handler.on_write();
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
    auto it = handlers_.find(fd);
    
    if (it == handlers_.end()) {
        // 首次注册此 fd
        EventHandler handler;
        handler.on_read = std::move(callback);
        handlers_[fd] = std::move(handler);
        
        // 添加到 epoll，使用边缘触发模式
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;  // 读事件 + 边缘触发
        ev.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            Logger::instance().error("Failed to add fd={} to epoll: {}", fd, std::strerror(errno));
            handlers_.erase(fd);
            return false;
        }
        
        Logger::instance().debug("Registered read event for fd={}", fd);
    } else {
        // 更新已存在的 fd
        it->second.on_read = std::move(callback);
        
        // 修改 epoll 事件
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        if (it->second.on_write) {
            ev.events |= EPOLLOUT;
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
    auto it = handlers_.find(fd);
    
    if (it == handlers_.end()) {
        // 首次注册此 fd
        EventHandler handler;
        handler.on_write = std::move(callback);
        handlers_[fd] = std::move(handler);
        
        // 添加到 epoll，使用边缘触发模式
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;  // 写事件 + 边缘触发
        ev.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            Logger::instance().error("Failed to add fd={} to epoll: {}", fd, std::strerror(errno));
            handlers_.erase(fd);
            return false;
        }
        
        Logger::instance().debug("Registered write event for fd={}", fd);
    } else {
        // 更新已存在的 fd
        it->second.on_write = std::move(callback);
        
        // 修改 epoll 事件
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        if (it->second.on_read) {
            ev.events |= EPOLLIN;
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
    auto it = handlers_.find(fd);
    if (it == handlers_.end()) {
        Logger::instance().warn("Attempted to unregister non-existent fd={}", fd);
        return;
    }
    
    // 从 epoll 中移除
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        Logger::instance().error("Failed to remove fd={} from epoll: {}", fd, std::strerror(errno));
    }
    
    // 从处理器映射中移除
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

