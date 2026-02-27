#include "frpc/network_awaitable.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include "frpc/logger.h"
#include "frpc/exceptions.h"

namespace frpc {

// ============================================================================
// SendAwaitable Implementation
// ============================================================================

SendAwaitable::SendAwaitable(NetworkEngine* engine, int fd, std::span<const uint8_t> data)
    : engine_(engine)
    , fd_(fd)
    , data_(data)
    , completed_(false)
    , result_{false, 0, 0} {
}

bool SendAwaitable::await_ready() const noexcept {
    // 尝试立即发送
    return const_cast<SendAwaitable*>(this)->try_send();
}

void SendAwaitable::await_suspend(std::coroutine_handle<> handle) {
    handle_ = handle;
    
    // 注册写事件，当 socket 可写时恢复协程
    engine_->register_write_event(fd_, [this]() {
        on_write_ready();
    });
}

SendResult SendAwaitable::await_resume() const {
    return result_;
}

bool SendAwaitable::try_send() {
    if (data_.empty()) {
        result_ = {true, 0, 0};
        completed_ = true;
        return true;
    }
    
    ssize_t sent = ::send(fd_, data_.data(), data_.size(), MSG_DONTWAIT);
    
    if (sent > 0) {
        // 发送成功
        result_ = {true, sent, 0};
        completed_ = true;
        LOG_DEBUG("NetworkEngine", "Sent {} bytes immediately on fd={}", sent, fd_);
        return true;
    } else if (sent == 0) {
        // 连接关闭
        result_ = {false, 0, ECONNRESET};
        completed_ = true;
        LOG_WARN("NetworkEngine", "Connection closed on fd={}", fd_);
        return true;
    } else {
        // 错误
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            // 需要等待
            LOG_DEBUG("NetworkEngine", "Send would block on fd={}, suspending", fd_);
            return false;
        } else {
            // 其他错误
            result_ = {false, 0, err};
            completed_ = true;
            LOG_ERROR("NetworkEngine", "Send failed on fd={}: {}", fd_, std::strerror(err));
            return true;
        }
    }
}

void SendAwaitable::on_write_ready() {
    // Socket 可写，尝试发送
    if (try_send()) {
        // 发送完成，注销写事件
        engine_->unregister_event(fd_);
        
        // 恢复协程
        if (handle_) {
            handle_.resume();
        }
    }
}

// ============================================================================
// RecvAwaitable Implementation
// ============================================================================

RecvAwaitable::RecvAwaitable(NetworkEngine* engine, int fd, std::span<uint8_t> buffer)
    : engine_(engine)
    , fd_(fd)
    , buffer_(buffer)
    , completed_(false)
    , bytes_received_(0) {
}

bool RecvAwaitable::await_ready() const noexcept {
    // 尝试立即接收
    return const_cast<RecvAwaitable*>(this)->try_recv();
}

void RecvAwaitable::await_suspend(std::coroutine_handle<> handle) {
    handle_ = handle;
    
    // 注册读事件，当有数据可读时恢复协程
    engine_->register_read_event(fd_, [this]() {
        on_read_ready();
    });
}

ssize_t RecvAwaitable::await_resume() const {
    return bytes_received_;
}

bool RecvAwaitable::try_recv() {
    if (buffer_.empty()) {
        bytes_received_ = 0;
        completed_ = true;
        return true;
    }
    
    ssize_t received = ::recv(fd_, buffer_.data(), buffer_.size(), MSG_DONTWAIT);
    
    if (received > 0) {
        // 接收成功
        bytes_received_ = received;
        completed_ = true;
        LOG_DEBUG("NetworkEngine", "Received {} bytes immediately on fd={}", received, fd_);
        return true;
    } else if (received == 0) {
        // 连接关闭
        bytes_received_ = 0;
        completed_ = true;
        LOG_INFO("NetworkEngine", "Connection closed by peer on fd={}", fd_);
        return true;
    } else {
        // 错误
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            // 需要等待
            LOG_DEBUG("NetworkEngine", "Recv would block on fd={}, suspending", fd_);
            return false;
        } else {
            // 其他错误
            bytes_received_ = -1;
            completed_ = true;
            LOG_ERROR("NetworkEngine", "Recv failed on fd={}: {}", fd_, std::strerror(err));
            return true;
        }
    }
}

void RecvAwaitable::on_read_ready() {
    // 有数据可读，尝试接收
    if (try_recv()) {
        // 接收完成，注销读事件
        engine_->unregister_event(fd_);
        
        // 恢复协程
        if (handle_) {
            handle_.resume();
        }
    }
}

// ============================================================================
// ConnectAwaitable Implementation
// ============================================================================

ConnectAwaitable::ConnectAwaitable(NetworkEngine* engine, const std::string& addr, uint16_t port)
    : engine_(engine)
    , addr_(addr)
    , port_(port)
    , completed_(false)
    , fd_(-1)
    , error_code_(0) {
}

bool ConnectAwaitable::await_ready() const noexcept {
    // 尝试立即连接
    return const_cast<ConnectAwaitable*>(this)->try_connect();
}

void ConnectAwaitable::await_suspend(std::coroutine_handle<> handle) {
    handle_ = handle;
    
    // 注册写事件，当连接建立时 socket 变为可写
    engine_->register_write_event(fd_, [this]() {
        on_connect_ready();
    });
}

int ConnectAwaitable::await_resume() const {
    if (error_code_ != 0) {
        throw NetworkException(
            ErrorCode::ConnectionFailed,
            "Connection failed: " + std::string(std::strerror(error_code_))
        );
    }
    return fd_;
}

bool ConnectAwaitable::try_connect() {
    // 创建 socket
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        error_code_ = errno;
        completed_ = true;
        LOG_ERROR("NetworkEngine", "Failed to create socket: {}", std::strerror(error_code_));
        return true;
    }
    
    // 设置为非阻塞模式
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1 || fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        error_code_ = errno;
        completed_ = true;
        close(fd_);
        fd_ = -1;
        LOG_ERROR("NetworkEngine", "Failed to set non-blocking: {}", std::strerror(error_code_));
        return true;
    }
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, addr_.c_str(), &server_addr.sin_addr) <= 0) {
        error_code_ = EINVAL;
        completed_ = true;
        close(fd_);
        fd_ = -1;
        LOG_ERROR("NetworkEngine", "Invalid address: {}", addr_);
        return true;
    }
    
    // 发起连接
    int result = connect(fd_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
    
    if (result == 0) {
        // 连接立即成功（罕见）
        completed_ = true;
        LOG_INFO("NetworkEngine", "Connected immediately to {}:{}, fd={}", addr_, port_, fd_);
        return true;
    } else {
        int err = errno;
        if (err == EINPROGRESS) {
            // 连接正在进行中，需要等待
            LOG_DEBUG("NetworkEngine", "Connection in progress to {}:{}, fd={}", addr_, port_, fd_);
            return false;
        } else {
            // 连接失败
            error_code_ = err;
            completed_ = true;
            close(fd_);
            fd_ = -1;
            LOG_ERROR("ConnectAwaitable", "Connect failed to {}:{}: {}", addr_, port_, std::strerror(err));
            return true;
        }
    }
}

void ConnectAwaitable::on_connect_ready() {
    // Socket 可写，检查连接是否成功
    int error = 0;
    socklen_t len = sizeof(error);
    
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        error_code_ = errno;
        LOG_ERROR("ConnectAwaitable", "getsockopt failed on fd={}: {}", fd_, std::strerror(error_code_));
    } else if (error != 0) {
        error_code_ = error;
        LOG_ERROR("ConnectAwaitable", "Connection failed to {}:{}: {}", addr_, port_, std::strerror(error));
    } else {
        // 连接成功
        LOG_INFO("ConnectAwaitable", "Connected to {}:{}, fd={}", addr_, port_, fd_);
    }
    
    completed_ = true;
    
    // 注销写事件
    engine_->unregister_event(fd_);
    
    // 恢复协程
    if (handle_) {
        handle_.resume();
    }
}

}  // namespace frpc

