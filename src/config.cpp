/**
 * @file config.cpp
 * @brief FRPC 框架配置管理实现
 * 
 * 实现配置文件的加载和解析功能。
 */

#include "frpc/config.h"
#include "frpc/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace frpc {

namespace {

/**
 * @brief 简单的 JSON 解析辅助函数
 * 
 * 从 JSON 字符串中提取指定键的字符串值。
 * 这是一个简化的实现，仅支持基本的 JSON 格式。
 * 
 * @param json JSON 字符串
 * @param key 要查找的键
 * @return 键对应的值，如果未找到则返回空字符串
 */
std::string extract_string_value(const std::string& json, const std::string& key) {
    // 查找键
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) {
        return "";
    }
    
    // 查找冒号
    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return "";
    }
    
    // 跳过空白字符
    size_t value_start = colon_pos + 1;
    while (value_start < json.size() && std::isspace(json[value_start])) {
        ++value_start;
    }
    
    // 提取值
    if (value_start >= json.size()) {
        return "";
    }
    
    // 如果是字符串值（带引号）
    if (json[value_start] == '"') {
        size_t value_end = json.find('"', value_start + 1);
        if (value_end == std::string::npos) {
            return "";
        }
        return json.substr(value_start + 1, value_end - value_start - 1);
    }
    
    // 如果是数字或布尔值（不带引号）
    size_t value_end = value_start;
    while (value_end < json.size() && 
           json[value_end] != ',' && 
           json[value_end] != '}' && 
           json[value_end] != '\n' &&
           json[value_end] != '\r') {
        ++value_end;
    }
    
    std::string value = json.substr(value_start, value_end - value_start);
    // 移除尾部空白
    while (!value.empty() && std::isspace(value.back())) {
        value.pop_back();
    }
    
    return value;
}

/**
 * @brief 从字符串转换为整数
 * 
 * @param str 字符串
 * @param default_value 转换失败时的默认值
 * @return 转换后的整数值
 */
template<typename T>
T string_to_int(const std::string& str, T default_value) {
    if (str.empty()) {
        return default_value;
    }
    
    try {
        if constexpr (std::is_same_v<T, uint16_t>) {
            return static_cast<uint16_t>(std::stoul(str));
        } else if constexpr (std::is_same_v<T, size_t>) {
            return std::stoull(str);
        } else if constexpr (std::is_same_v<T, int>) {
            return std::stoi(str);
        } else {
            return static_cast<T>(std::stoll(str));
        }
    } catch (...) {
        return default_value;
    }
}

} // anonymous namespace

ServerConfig ServerConfig::load_from_file(const std::string& path) {
    ServerConfig config;  // 使用默认值初始化
    
    // 尝试打开配置文件
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_WARN("Config", "Failed to open server config file: {}, using default config", path);
        return config;
    }
    
    // 读取文件内容
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    
    if (json.empty()) {
        LOG_WARN("Config", "Server config file is empty: {}, using default config", path);
        return config;
    }
    
    // 解析配置项
    try {
        std::string value;
        
        // 解析 listen_addr
        value = extract_string_value(json, "listen_addr");
        if (!value.empty()) {
            config.listen_addr = value;
        }
        
        // 解析 listen_port
        value = extract_string_value(json, "listen_port");
        if (!value.empty()) {
            config.listen_port = string_to_int<uint16_t>(value, config.listen_port);
        }
        
        // 解析 max_connections
        value = extract_string_value(json, "max_connections");
        if (!value.empty()) {
            config.max_connections = string_to_int<size_t>(value, config.max_connections);
        }
        
        // 解析 idle_timeout_seconds
        value = extract_string_value(json, "idle_timeout_seconds");
        if (!value.empty()) {
            int seconds = string_to_int<int>(value, 300);
            config.idle_timeout = std::chrono::seconds(seconds);
        }
        
        // 解析 worker_threads
        value = extract_string_value(json, "worker_threads");
        if (!value.empty()) {
            config.worker_threads = string_to_int<size_t>(value, config.worker_threads);
        }
        
        LOG_INFO("Config", "Loaded server config from file: {}", path);
    } catch (const std::exception& e) {
        LOG_WARN("Config", "Failed to parse server config file: {}, error: {}, using default config", 
                    path, e.what());
    }
    
    return config;
}

ClientConfig ClientConfig::load_from_file(const std::string& path) {
    ClientConfig config;  // 使用默认值初始化
    
    // 尝试打开配置文件
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_WARN("Config", "Failed to open client config file: {}, using default config", path);
        return config;
    }
    
    // 读取文件内容
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    
    if (json.empty()) {
        LOG_WARN("Config", "Client config file is empty: {}, using default config", path);
        return config;
    }
    
    // 解析配置项
    try {
        std::string value;
        
        // 解析 default_timeout_ms
        value = extract_string_value(json, "default_timeout_ms");
        if (!value.empty()) {
            int timeout_ms = string_to_int<int>(value, 5000);
            config.default_timeout = std::chrono::milliseconds(timeout_ms);
        }
        
        // 解析 max_retries
        value = extract_string_value(json, "max_retries");
        if (!value.empty()) {
            config.max_retries = string_to_int<size_t>(value, config.max_retries);
        }
        
        // 解析 load_balance_strategy
        value = extract_string_value(json, "load_balance_strategy");
        if (!value.empty()) {
            config.load_balance_strategy = value;
        }
        
        LOG_INFO("Config", "Loaded client config from file: {}", path);
    } catch (const std::exception& e) {
        LOG_WARN("Config", "Failed to parse client config file: {}, error: {}, using default config", 
                    path, e.what());
    }
    
    return config;
}

} // namespace frpc
