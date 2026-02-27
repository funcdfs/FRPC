/**
 * @file health_checker.cpp
 * @brief 健康检测器实现
 */

#include "frpc/health_checker.h"
#include "frpc/logger.h"
#include <algorithm>
#include <thread>

namespace frpc {

HealthChecker::HealthChecker(const HealthCheckConfig& config, ServiceRegistry* registry)
    : config_(config), registry_(registry) {
    if (!registry_) {
        throw std::invalid_argument("ServiceRegistry pointer cannot be null");
    }
    
    // 验证配置参数
    if (config_.interval.count() <= 0) {
        throw std::invalid_argument("Health check interval must be positive");
    }
    if (config_.timeout.count() <= 0) {
        throw std::invalid_argument("Health check timeout must be positive");
    }
    if (config_.timeout >= config_.interval) {
        LOG_WARN("HealthChecker", "Health check timeout ({} s) should be less than interval ({} s)",
                    config_.timeout.count(), config_.interval.count());
    }
    if (config_.failure_threshold <= 0) {
        throw std::invalid_argument("Failure threshold must be positive");
    }
    if (config_.success_threshold <= 0) {
        throw std::invalid_argument("Success threshold must be positive");
    }
    
    // 注意：在非Linux平台上，NetworkEngine可能不可用
    // 这里不初始化engine_，使用简化的健康检测实现
    
    LOG_INFO("HealthChecker", "HealthChecker created with interval={}s, timeout={}s, "
                "failure_threshold={}, success_threshold={}",
                config_.interval.count(), config_.timeout.count(),
                config_.failure_threshold, config_.success_threshold);
}

HealthChecker::~HealthChecker() {
    stop();
}

void HealthChecker::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        LOG_WARN("HealthChecker", "HealthChecker is already running");
        return;
    }
    
    LOG_INFO("HealthChecker", "Starting HealthChecker");
    
    // 启动检测循环协程
    // 注意：这里简化实现，实际应该由协程调度器管理
    // 在实际应用中，应该将协程句柄保存并由调度器调度
    auto task = check_loop();
    // 协程会在后台运行
    
    LOG_INFO("HealthChecker", "HealthChecker started");
}

void HealthChecker::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        LOG_DEBUG("HealthChecker", "HealthChecker is not running");
        return;
    }
    
    LOG_INFO("HealthChecker", "Stopping HealthChecker");
    
    // 等待检测循环结束
    // 注意：这里简化实现，实际应该等待协程完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    LOG_INFO("HealthChecker", "HealthChecker stopped");
}

void HealthChecker::add_target(const std::string& service_name, 
                               const ServiceInstance& instance) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否已存在
    TargetInfo new_target{
        .service_name = service_name,
        .instance = instance,
        .consecutive_failures = 0,
        .consecutive_successes = 0,
        .currently_healthy = true
    };
    
    auto it = std::find(targets_.begin(), targets_.end(), new_target);
    if (it != targets_.end()) {
        LOG_DEBUG("HealthChecker", "Target already exists: {}:{}:{}", 
                     service_name, instance.host, instance.port);
        return;
    }
    
    targets_.push_back(new_target);
    LOG_INFO("HealthChecker", "Added health check target: {}:{}:{}", 
                service_name, instance.host, instance.port);
}

void HealthChecker::remove_target(const std::string& service_name,
                                  const ServiceInstance& instance) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    TargetInfo target_to_remove{
        .service_name = service_name,
        .instance = instance
    };
    
    auto it = std::find(targets_.begin(), targets_.end(), target_to_remove);
    if (it != targets_.end()) {
        targets_.erase(it);
        LOG_INFO("HealthChecker", "Removed health check target: {}:{}:{}", 
                    service_name, instance.host, instance.port);
    } else {
        LOG_DEBUG("HealthChecker", "Target not found: {}:{}:{}", 
                     service_name, instance.host, instance.port);
    }
}

RpcTask<void> HealthChecker::check_loop() {
    LOG_INFO("HealthChecker", "Health check loop started");
    
    while (running_.load()) {
        // 等待检测间隔
        co_await std::suspend_always{};
        std::this_thread::sleep_for(config_.interval);
        
        if (!running_.load()) {
            break;
        }
        
        LOG_DEBUG("HealthChecker", "Starting health check cycle");
        
        // 获取目标列表的副本，避免长时间持有锁
        std::vector<TargetInfo> targets_copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            targets_copy = targets_;
        }
        
        // 检测每个目标实例
        for (size_t i = 0; i < targets_copy.size(); ++i) {
            auto& target = targets_copy[i];
            
            LOG_DEBUG("HealthChecker", "Checking instance: {}:{}:{}", 
                         target.service_name, target.instance.host, target.instance.port);
            
            // 检测实例
            bool is_healthy = co_await check_instance(target.instance);
            
            // 更新状态
            if (is_healthy) {
                // 检测成功
                target.consecutive_failures = 0;
                target.consecutive_successes++;
                
                LOG_DEBUG("HealthChecker", "Health check succeeded: {}:{}:{} (consecutive_successes={})",
                            target.service_name, target.instance.host, target.instance.port,
                            target.consecutive_successes);
                
                // 如果之前是不健康状态，且连续成功次数达到阈值，标记为健康
                if (!target.currently_healthy && 
                    target.consecutive_successes >= config_.success_threshold) {
                    target.currently_healthy = true;
                    target.consecutive_successes = 0;
                    
                    LOG_INFO("HealthChecker", "Instance recovered: {}:{}:{}", 
                               target.service_name, target.instance.host, target.instance.port);
                    
                    // 通知服务注册中心
                    registry_->update_health_status(target.service_name, 
                                                   target.instance, true);
                }
            } else {
                // 检测失败
                target.consecutive_successes = 0;
                target.consecutive_failures++;
                
                LOG_DEBUG("HealthChecker", "Health check failed: {}:{}:{} (consecutive_failures={})",
                            target.service_name, target.instance.host, target.instance.port,
                            target.consecutive_failures);
                
                // 如果之前是健康状态，且连续失败次数达到阈值，标记为不健康
                if (target.currently_healthy && 
                    target.consecutive_failures >= config_.failure_threshold) {
                    target.currently_healthy = false;
                    target.consecutive_failures = 0;
                    
                    LOG_WARN("HealthChecker", "Instance marked unhealthy: {}:{}:{}", 
                               target.service_name, target.instance.host, target.instance.port);
                    
                    // 通知服务注册中心
                    registry_->update_health_status(target.service_name, 
                                                   target.instance, false);
                }
            }
            
            // 更新目标列表中的状态
            {
                std::lock_guard<std::mutex> lock(mutex_);
                // 查找对应的目标并更新
                for (auto& t : targets_) {
                    if (t == target) {
                        t.consecutive_failures = target.consecutive_failures;
                        t.consecutive_successes = target.consecutive_successes;
                        t.currently_healthy = target.currently_healthy;
                        break;
                    }
                }
            }
        }
        
        LOG_DEBUG("HealthChecker", "Health check cycle completed");
    }
    
    LOG_INFO("HealthChecker", "Health check loop stopped");
    co_return;
}

RpcTask<bool> HealthChecker::check_instance(const ServiceInstance& instance) {
    try {
        LOG_DEBUG("HealthChecker", "Attempting to connect to {}:{}", instance.host, instance.port);
        
        // 简化的健康检测实现
        // 注意：在实际应用中，应该使用 NetworkEngine 的 async_connect 进行真实的连接测试
        // 这里使用简化的模拟实现，用于测试框架功能
        
        // 模拟异步等待
        co_await std::suspend_always{};
        
        // 模拟连接测试
        // 在实际实现中，这里应该：
        // 1. 使用 NetworkEngine 的 async_connect 建立连接
        // 2. 设置超时定时器
        // 3. 等待连接完成或超时
        // 4. 关闭连接
        
        // 简化实现：假设连接总是失败（因为没有真实的服务器）
        // 这样可以测试健康检测的失败处理逻辑
        LOG_DEBUG("HealthChecker", "Connection failed (simplified implementation): {}:{}", 
                 instance.host, instance.port);
        co_return false;
        
    } catch (const std::exception& e) {
        LOG_ERROR("HealthChecker", "Health check exception for {}:{}: {}", 
                     instance.host, instance.port, e.what());
        co_return false;
    }
}

} // namespace frpc
