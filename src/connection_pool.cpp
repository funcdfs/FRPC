/**
 * @file connection_pool.cpp
 * @brief 连接池管理器和连接类实现
 */

#include "frpc/connection_pool.h"
#include "frpc/network_awaitable.h"
#include "frpc/exceptions.h"
#include "frpc/logger.h"
#include <unistd.h>
#include <algorithm>

namespace frpc {

// ============================================================================
// Connection Implementation
// ============================================================================

Connection::Connection(int fd, const ServiceInstance& instance, NetworkEngine* engine)
    : fd_(fd)
    , instance_(instance)
    , engine_(engine)
    , created_at_(std::chrono::steady_clock::now())
    , last_used_at_(std::chrono::steady_clock::now())
{
    if (fd_ <= 0) {
        throw std::invalid_argument("Invalid file descriptor");
    }
    if (!engine_) {
        throw std::invalid_argument("NetworkEngine pointer cannot be null");
    }
}

Connection::~Connection() {
    if (fd_ > 0) {
        // 注销网络事件
        engine_->unregister_event(fd_);
        // 关闭 socket
        ::close(fd_);
        fd_ = -1;
    }
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_)
    , instance_(std::move(other.instance_))
    , engine_(other.engine_)
    , created_at_(other.created_at_)
    , last_used_at_(other.last_used_at_)
{
    other.fd_ = -1;
    other.engine_ = nullptr;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        // 关闭当前连接
        if (fd_ > 0) {
            engine_->unregister_event(fd_);
            ::close(fd_);
        }
        
        // 移动资源
        fd_ = other.fd_;
        instance_ = std::move(other.instance_);
        engine_ = other.engine_;
        created_at_ = other.created_at_;
        last_used_at_ = other.last_used_at_;
        
        // 清空源对象
        other.fd_ = -1;
        other.engine_ = nullptr;
    }
    return *this;
}

RpcTask<SendResult> Connection::send(std::span<const uint8_t> data) {
    // 更新最后使用时间
    last_used_at_ = std::chrono::steady_clock::now();
    
    // 检查连接是否有效
    if (!is_valid()) {
        SendResult result{
            .success = false,
            .bytes_sent = 0,
            .error_code = static_cast<int>(ErrorCode::ConnectionClosed)
        };
        co_return result;
    }
    
    // 委托给 NetworkEngine 进行异步发送
    try {
        auto result = co_await engine_->async_send(fd_, data);
        co_return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Connection", "Connection send failed: {}", e.what());
        SendResult result{
            .success = false,
            .bytes_sent = 0,
            .error_code = static_cast<int>(ErrorCode::SendFailed)
        };
        co_return result;
    }
}

RpcTask<std::vector<uint8_t>> Connection::recv() {
    // 更新最后使用时间
    last_used_at_ = std::chrono::steady_clock::now();
    
    // 检查连接是否有效
    if (!is_valid()) {
        co_return std::vector<uint8_t>{};
    }
    
    // 委托给 NetworkEngine 进行异步接收
    try {
        // 创建接收缓冲区
        std::vector<uint8_t> buffer(8192);  // 8KB 缓冲区
        
        ssize_t bytes = co_await engine_->async_recv(fd_, buffer);
        
        if (bytes > 0) {
            // 调整缓冲区大小
            buffer.resize(bytes);
            co_return buffer;
        } else if (bytes == 0) {
            // 连接关闭
            LOG_INFO("Connection", "Connection closed by remote: {}:{}", instance_.host, instance_.port);
            co_return std::vector<uint8_t>{};
        } else {
            // 错误
            LOG_ERROR("Connection", "Connection recv failed: {}:{}", instance_.host, instance_.port);
            co_return std::vector<uint8_t>{};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Connection", "Connection recv exception: {}", e.what());
        co_return std::vector<uint8_t>{};
    }
}

bool Connection::is_valid() const {
    return fd_ > 0 && engine_ != nullptr;
}

// ============================================================================
// ConnectionPool Implementation
// ============================================================================

ConnectionPool::ConnectionPool(const PoolConfig& config, NetworkEngine* engine)
    : config_(config)
    , engine_(engine)
{
    if (!engine_) {
        throw std::invalid_argument("NetworkEngine pointer cannot be null");
    }
    
    LOG_INFO("ConnectionPool", "ConnectionPool created with config: min={}, max={}, idle_timeout={}s",
             config_.min_connections, config_.max_connections, config_.idle_timeout.count());
}

ConnectionPool::~ConnectionPool() {
    // 关闭所有连接
    std::unique_lock lock(mutex_);
    
    for (auto& [instance, pool] : pools_) {
        pool.idle_connections.clear();
        pool.active_connections.clear();
    }
    
    LOG_INFO("ConnectionPool", "ConnectionPool destroyed");
}

RpcTask<Connection> ConnectionPool::get_connection(const ServiceInstance& instance) {
    // 首先尝试获取空闲连接（使用读锁）
    {
        std::shared_lock lock(mutex_);
        
        auto it = pools_.find(instance);
        if (it != pools_.end() && !it->second.idle_connections.empty()) {
            // 有空闲连接，优先返回
            LOG_DEBUG("ConnectionPool", "Reusing idle connection to {}:{}", instance.host, instance.port);
            
            // 需要升级为写锁来修改连接池
            lock.unlock();
            std::unique_lock write_lock(mutex_);
            
            // 再次检查（可能被其他线程取走）
            it = pools_.find(instance);
            if (it != pools_.end() && !it->second.idle_connections.empty()) {
                // 从空闲列表移动到活跃列表
                Connection conn = std::move(it->second.idle_connections.back());
                it->second.idle_connections.pop_back();
                
                // 检查连接是否仍然有效
                if (conn.is_valid()) {
                    co_return std::move(conn);
                } else {
                    // 连接已失效，继续创建新连接
                    LOG_WARN("ConnectionPool", "Idle connection to {}:{} is invalid, creating new one",
                            instance.host, instance.port);
                    it->second.total_count--;
                }
            }
        }
    }
    
    // 没有空闲连接，检查是否可以创建新连接
    {
        std::unique_lock lock(mutex_);
        
        auto& pool = pools_[instance];
        
        if (pool.total_count >= config_.max_connections) {
            // 达到最大连接数，返回错误
            LOG_ERROR("ConnectionPool", "Connection pool exhausted for {}:{} (max={})",
                     instance.host, instance.port, config_.max_connections);
            throw ResourceExhaustedException(
                "Connection pool exhausted for " + instance.host + ":" + std::to_string(instance.port));
        }
        
        // 可以创建新连接
        pool.total_count++;
    }
    
    // 创建新连接（不持有锁，避免阻塞其他线程）
    try {
        LOG_DEBUG("ConnectionPool", "Creating new connection to {}:{}", instance.host, instance.port);
        auto conn = co_await create_connection(instance);
        co_return std::move(conn);
    } catch (const std::exception& e) {
        // 创建失败，减少计数
        std::unique_lock lock(mutex_);
        auto& pool = pools_[instance];
        pool.total_count--;
        
        LOG_ERROR("ConnectionPool", "Failed to create connection to {}:{}: {}",
                 instance.host, instance.port, e.what());
        throw;
    }
}

void ConnectionPool::return_connection(Connection conn) {
    if (!conn.is_valid()) {
        // 连接已失效，直接丢弃
        LOG_DEBUG("ConnectionPool", "Discarding invalid connection to {}:{}",
                 conn.instance().host, conn.instance().port);
        
        std::unique_lock lock(mutex_);
        auto& pool = pools_[conn.instance()];
        pool.total_count--;
        return;
    }
    
    // 连接有效，放回空闲列表
    std::unique_lock lock(mutex_);
    
    auto& pool = pools_[conn.instance()];
    pool.idle_connections.push_back(std::move(conn));
    
    LOG_DEBUG("ConnectionPool", "Connection returned to pool: {}:{} (idle={}, total={})",
             conn.instance().host, conn.instance().port,
             pool.idle_connections.size(), pool.total_count);
}

void ConnectionPool::remove_instance(const ServiceInstance& instance) {
    std::unique_lock lock(mutex_);
    
    auto it = pools_.find(instance);
    if (it != pools_.end()) {
        LOG_INFO("ConnectionPool", "Removing all connections to {}:{} (total={})",
                instance.host, instance.port, it->second.total_count);
        
        // 清空连接列表（析构函数会自动关闭连接）
        it->second.idle_connections.clear();
        it->second.active_connections.clear();
        it->second.total_count = 0;
        
        // 移除实例池
        pools_.erase(it);
    }
}

PoolStats ConnectionPool::get_stats() const {
    std::shared_lock lock(mutex_);
    
    PoolStats stats;
    
    for (const auto& [instance, pool] : pools_) {
        stats.total_connections += pool.total_count;
        stats.idle_connections += pool.idle_connections.size();
    }
    
    stats.active_connections = stats.total_connections - stats.idle_connections;
    
    // 计算连接复用率（简化版本）
    if (stats.total_connections > 0) {
        stats.connection_reuse_rate = static_cast<double>(stats.idle_connections) / stats.total_connections;
    }
    
    return stats;
}

void ConnectionPool::cleanup_idle_connections() {
    std::unique_lock lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    size_t removed_count = 0;
    
    for (auto& [instance, pool] : pools_) {
        // 移除空闲超时的连接
        auto it = pool.idle_connections.begin();
        while (it != pool.idle_connections.end()) {
            auto idle_time = now - it->last_used_at();
            
            if (idle_time > config_.idle_timeout || !it->is_valid()) {
                LOG_DEBUG("ConnectionPool", "Removing idle connection to {}:{} (idle_time={}s, valid={})",
                         instance.host, instance.port,
                         std::chrono::duration_cast<std::chrono::seconds>(idle_time).count(),
                         it->is_valid());
                
                it = pool.idle_connections.erase(it);
                pool.total_count--;
                removed_count++;
            } else {
                ++it;
            }
        }
    }
    
    if (removed_count > 0) {
        LOG_INFO("ConnectionPool", "Cleaned up {} idle connections", removed_count);
    }
}

RpcTask<Connection> ConnectionPool::create_connection(const ServiceInstance& instance) {
    // 使用 NetworkEngine 建立连接
    try {
        int fd = co_await engine_->async_connect(instance.host, instance.port);
        
        LOG_INFO("ConnectionPool", "Connected to {}:{}, fd={}", instance.host, instance.port, fd);
        
        // 创建 Connection 对象
        Connection conn(fd, instance, engine_);
        co_return std::move(conn);
    } catch (const std::exception& e) {
        LOG_ERROR("ConnectionPool", "Failed to connect to {}:{}: {}", instance.host, instance.port, e.what());
        throw ConnectionFailedException("Failed to connect to " + instance.host + ":" + 
                                       std::to_string(instance.port) + ": " + e.what());
    }
}

}  // namespace frpc
