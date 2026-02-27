/**
 * @file service_registry.cpp
 * @brief 服务注册中心实现
 */

#include "frpc/service_registry.h"
#include "frpc/logger.h"
#include <algorithm>

namespace frpc {

bool ServiceRegistry::register_service(const std::string& service_name, 
                                      const ServiceInstance& instance) {
    // 获取写锁
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 获取或创建服务的实例列表
    auto& instances = services_[service_name];
    
    // 检查实例是否已存在
    auto it = std::find_if(instances.begin(), instances.end(),
        [&instance](const InstanceInfo& info) {
            return info.instance == instance;
        });
    
    if (it != instances.end()) {
        // 实例已存在，更新信息
        it->instance.weight = instance.weight;
        it->healthy = true;
        it->last_heartbeat = std::chrono::steady_clock::now();
        
        LOG_INFO("ServiceRegistry", "Service instance updated: service={}, host={}, port={}",
                    service_name, instance.host, instance.port);
    } else {
        // 新实例，添加到列表
        InstanceInfo info;
        info.instance = instance;
        info.healthy = true;
        info.last_heartbeat = std::chrono::steady_clock::now();
        instances.push_back(info);
        
        LOG_INFO("ServiceRegistry", "Service instance registered: service={}, host={}, port={}, weight={}",
                    service_name, instance.host, instance.port, instance.weight);
    }
    
    // 通知订阅者
    notify_subscribers(service_name);
    
    return true;
}

void ServiceRegistry::unregister_service(const std::string& service_name,
                                        const ServiceInstance& instance) {
    // 获取写锁
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 查找服务
    auto service_it = services_.find(service_name);
    if (service_it == services_.end()) {
        // 服务不存在
        LOG_WARN("ServiceRegistry", "Attempted to unregister from non-existent service: service={}",
                    service_name);
        return;
    }
    
    // 查找并移除实例
    auto& instances = service_it->second;
    auto it = std::find_if(instances.begin(), instances.end(),
        [&instance](const InstanceInfo& info) {
            return info.instance == instance;
        });
    
    if (it != instances.end()) {
        instances.erase(it);
        
        LOG_INFO("ServiceRegistry", "Service instance unregistered: service={}, host={}, port={}",
                    service_name, instance.host, instance.port);
        
        // 如果服务没有实例了，移除服务
        if (instances.empty()) {
            services_.erase(service_it);
            LOG_INFO("ServiceRegistry", "Service removed (no instances left): service={}", service_name);
        }
        
        // 通知订阅者
        notify_subscribers(service_name);
    } else {
        LOG_WARN("ServiceRegistry", "Attempted to unregister non-existent instance: service={}, host={}, port={}",
                    service_name, instance.host, instance.port);
    }
}

std::vector<ServiceInstance> ServiceRegistry::get_instances(const std::string& service_name) const {
    // 获取读锁
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // 查找服务
    auto it = services_.find(service_name);
    if (it == services_.end()) {
        // 服务不存在，返回空列表
        LOG_DEBUG("ServiceRegistry", "Service not found: service={}", service_name);
        return {};
    }
    
    // 过滤出健康的实例
    std::vector<ServiceInstance> healthy_instances;
    for (const auto& info : it->second) {
        if (info.healthy) {
            healthy_instances.push_back(info.instance);
        }
    }
    
    LOG_DEBUG("ServiceRegistry", "Query service instances: service={}, total={}, healthy={}",
                 service_name, it->second.size(), healthy_instances.size());
    
    return healthy_instances;
}

void ServiceRegistry::subscribe(const std::string& service_name, 
                               ChangeCallback callback) {
    // 获取写锁
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 添加订阅者
    subscribers_[service_name].push_back(std::move(callback));
    
    LOG_INFO("ServiceRegistry", "Subscriber added: service={}, total_subscribers={}",
                service_name, subscribers_[service_name].size());
}

void ServiceRegistry::update_health_status(const std::string& service_name,
                                          const ServiceInstance& instance,
                                          bool healthy) {
    // 获取写锁
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 查找服务
    auto service_it = services_.find(service_name);
    if (service_it == services_.end()) {
        LOG_WARN("ServiceRegistry", "Attempted to update health status for non-existent service: service={}",
                    service_name);
        return;
    }
    
    // 查找实例
    auto& instances = service_it->second;
    auto it = std::find_if(instances.begin(), instances.end(),
        [&instance](const InstanceInfo& info) {
            return info.instance == instance;
        });
    
    if (it != instances.end()) {
        bool status_changed = (it->healthy != healthy);
        
        // 更新健康状态和心跳时间
        it->healthy = healthy;
        it->last_heartbeat = std::chrono::steady_clock::now();
        
        LOG_INFO("ServiceRegistry", "Health status updated: service={}, host={}, port={}, healthy={}",
                    service_name, instance.host, instance.port, healthy);
        
        // 只有状态发生变化时才通知订阅者
        if (status_changed) {
            LOG_INFO("ServiceRegistry", "Health status changed, notifying subscribers: service={}, healthy={}",
                        service_name, healthy);
            notify_subscribers(service_name);
        }
    } else {
        LOG_WARN("ServiceRegistry", "Attempted to update health status for non-existent instance: service={}, host={}, port={}",
                    service_name, instance.host, instance.port);
    }
}

void ServiceRegistry::notify_subscribers(const std::string& service_name) {
    // 注意：此方法假设调用者已经持有写锁
    
    // 获取健康的实例列表
    std::vector<ServiceInstance> healthy_instances;
    auto service_it = services_.find(service_name);
    if (service_it != services_.end()) {
        for (const auto& info : service_it->second) {
            if (info.healthy) {
                healthy_instances.push_back(info.instance);
            }
        }
    }
    
    // 查找订阅者
    auto sub_it = subscribers_.find(service_name);
    if (sub_it != subscribers_.end()) {
        // 调用所有订阅者的回调函数
        for (const auto& callback : sub_it->second) {
            try {
                callback(healthy_instances);
            } catch (const std::exception& e) {
                LOG_ERROR("ServiceRegistry", "Exception in subscriber callback: service={}, error={}",
                            service_name, e.what());
            } catch (...) {
                LOG_ERROR("ServiceRegistry", "Unknown exception in subscriber callback: service={}",
                            service_name);
            }
        }
        
        LOG_DEBUG("ServiceRegistry", "Notified {} subscribers for service: {}",
                     sub_it->second.size(), service_name);
    }
}

} // namespace frpc
