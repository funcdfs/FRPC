/**
 * @file logger.h
 * @brief FRPC 框架日志系统
 * 
 * 提供线程安全的日志记录功能，支持多种日志级别、格式化输出和多目标输出。
 * 
 * 日志格式: [时间戳] [级别] [线程ID] [协程ID] [模块] 消息内容
 * 
 * @example 基本使用
 * @code
 * // 设置日志级别
 * Logger::instance().set_level(LogLevel::INFO);
 * 
 * // 输出到控制台
 * Logger::instance().add_console_output();
 * 
 * // 输出到文件
 * Logger::instance().add_file_output("frpc.log");
 * 
 * // 记录日志
 * LOG_INFO("RpcServer", "Server started on port {}", 8080);
 * LOG_ERROR("RpcClient", "Connection failed: {}", error_msg);
 * @endcode
 * 
 * 验证需求: 12.3, 12.4, 13.4, 15.1
 */

#ifndef FRPC_LOGGER_H
#define FRPC_LOGGER_H

#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <fstream>
#include <vector>
#include <sstream>
#include <chrono>
#include <thread>
#include <iomanip>

namespace frpc {

/**
 * @brief 日志级别枚举
 * 
 * 日志级别从低到高：DEBUG < INFO < WARN < ERROR
 * 只有级别大于等于设置的最小级别的日志才会被输出。
 */
enum class LogLevel {
    DEBUG = 0,  ///< 调试信息 - 详细的调试信息，用于开发和问题诊断
    INFO = 1,   ///< 一般信息 - 正常的运行信息，如服务启动、连接建立
    WARN = 2,   ///< 警告信息 - 潜在的问题，如重试操作、性能降级
    ERROR = 3   ///< 错误信息 - 错误和异常情况，需要关注和处理
};

/**
 * @brief 将日志级别转换为字符串
 * 
 * @param level 日志级别
 * @return 日志级别的字符串表示
 */
inline std::string_view log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 日志输出目标接口
 * 
 * 所有日志输出目标都实现此接口，支持控制台、文件等多种输出方式。
 */
class LogOutput {
public:
    virtual ~LogOutput() = default;
    
    /**
     * @brief 写入日志消息
     * 
     * @param message 格式化后的日志消息
     */
    virtual void write(const std::string& message) = 0;
    
    /**
     * @brief 刷新输出缓冲区
     */
    virtual void flush() = 0;
};

/**
 * @brief 控制台日志输出
 * 
 * 将日志输出到标准错误流（stderr）。
 */
class ConsoleOutput : public LogOutput {
public:
    void write(const std::string& message) override;
    void flush() override;
};

/**
 * @brief 文件日志输出
 * 
 * 将日志输出到指定的文件。支持追加模式，多次运行不会覆盖之前的日志。
 */
class FileOutput : public LogOutput {
public:
    /**
     * @brief 构造函数
     * 
     * @param filename 日志文件路径
     * @throws std::runtime_error 如果无法打开文件
     */
    explicit FileOutput(const std::string& filename);
    
    ~FileOutput() override;
    
    void write(const std::string& message) override;
    void flush() override;

private:
    std::ofstream file_;
    std::mutex mutex_;  // 保护文件写入
};

/**
 * @brief 日志记录器（单例模式）
 * 
 * 提供线程安全的日志记录功能。使用单例模式确保全局只有一个日志实例。
 * 
 * 线程安全保证：
 * - 所有公共方法都使用互斥锁保护
 * - 支持多线程并发写日志
 * - 文件输出使用独立的互斥锁
 * 
 * @note 线程安全：所有公共方法都是线程安全的
 * 
 * @example 配置和使用
 * @code
 * // 获取日志实例
 * auto& logger = Logger::instance();
 * 
 * // 设置日志级别（只输出 INFO 及以上级别）
 * logger.set_level(LogLevel::INFO);
 * 
 * // 添加输出目标
 * logger.add_console_output();
 * logger.add_file_output("app.log");
 * 
 * // 记录日志
 * logger.log(LogLevel::INFO, "Main", "Application started");
 * logger.log(LogLevel::ERROR, "Network", "Connection failed: {}", error);
 * 
 * // 使用便捷宏
 * LOG_DEBUG("Module", "Debug info: value={}", value);
 * LOG_INFO("Module", "Info message");
 * LOG_WARN("Module", "Warning: {}", warning);
 * LOG_ERROR("Module", "Error: {}", error);
 * @endcode
 */
class Logger {
public:
    /**
     * @brief 获取日志实例（单例）
     * 
     * @return 日志实例的引用
     */
    static Logger& instance();
    
    // 禁止拷贝和移动
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    
    /**
     * @brief 设置最小日志级别
     * 
     * 只有级别大于等于此级别的日志才会被输出。
     * 
     * @param level 最小日志级别
     * 
     * @example
     * // 只输出 WARN 和 ERROR 级别的日志
     * Logger::instance().set_level(LogLevel::WARN);
     */
    void set_level(LogLevel level);
    
    /**
     * @brief 获取当前日志级别
     * 
     * @return 当前的最小日志级别
     */
    LogLevel get_level() const;
    
    /**
     * @brief 添加控制台输出
     * 
     * 日志将输出到标准错误流（stderr）。
     */
    void add_console_output();
    
    /**
     * @brief 添加文件输出
     * 
     * 日志将追加到指定的文件。如果文件不存在则创建，如果存在则追加。
     * 
     * @param filename 日志文件路径
     * @throws std::runtime_error 如果无法打开文件
     * 
     * @example
     * Logger::instance().add_file_output("/var/log/frpc.log");
     */
    void add_file_output(const std::string& filename);
    
    /**
     * @brief 记录日志（不带格式化参数）
     * 
     * @param level 日志级别
     * @param module 模块名称（用于标识日志来源）
     * @param message 日志消息
     * 
     * @example
     * Logger::instance().log(LogLevel::INFO, "RpcServer", "Server started");
     */
    void log(LogLevel level, std::string_view module, std::string_view message);
    
    /**
     * @brief 记录日志（带格式化参数）
     * 
     * 使用类似 fmt 库的格式化语法，支持 {} 占位符。
     * 
     * @tparam Args 参数类型
     * @param level 日志级别
     * @param module 模块名称
     * @param format 格式化字符串
     * @param args 格式化参数
     * 
     * @example
     * Logger::instance().log(LogLevel::ERROR, "RpcClient", 
     *                       "Connection failed: host={}, port={}", host, port);
     */
    template<typename... Args>
    void log(LogLevel level, std::string_view module, 
             std::string_view format, Args&&... args) {
        if (level < min_level_) {
            return;
        }
        
        auto message = format_message(format, std::forward<Args>(args)...);
        log(level, module, message);
    }
    
    /**
     * @brief 刷新所有输出缓冲区
     * 
     * 确保所有日志都被写入到输出目标。
     */
    void flush();
    
    /**
     * @brief 设置协程 ID（用于协程上下文）
     * 
     * 在协程中调用此方法设置当前协程的 ID，后续的日志将包含此 ID。
     * 
     * @param coro_id 协程 ID
     * 
     * @note 使用 thread_local 存储，每个线程独立
     */
    static void set_coroutine_id(uint64_t coro_id);
    
    /**
     * @brief 获取当前协程 ID
     * 
     * @return 协程 ID，如果未设置则返回 0
     */
    static uint64_t get_coroutine_id();

private:
    Logger();
    ~Logger() = default;
    
    /**
     * @brief 格式化日志消息
     * 
     * 生成完整的日志行，包含时间戳、级别、线程 ID、协程 ID、模块名和消息。
     * 
     * 格式: [时间戳] [级别] [线程ID] [协程ID] [模块] 消息
     * 
     * @param level 日志级别
     * @param module 模块名称
     * @param message 日志消息
     * @return 格式化后的日志行
     */
    std::string format_log_line(LogLevel level, std::string_view module, 
                                std::string_view message);
    
    /**
     * @brief 格式化消息（简单的 {} 占位符替换）
     * 
     * @tparam Args 参数类型
     * @param format 格式化字符串
     * @param args 参数
     * @return 格式化后的消息
     */
    template<typename... Args>
    std::string format_message(std::string_view format, Args&&... args) {
        std::ostringstream oss;
        format_message_impl(oss, format, std::forward<Args>(args)...);
        return oss.str();
    }
    
    /**
     * @brief 格式化消息实现（递归终止）
     */
    void format_message_impl(std::ostringstream& oss, std::string_view format);
    
    /**
     * @brief 格式化消息实现（递归）
     */
    template<typename T, typename... Args>
    void format_message_impl(std::ostringstream& oss, std::string_view format, 
                            T&& value, Args&&... args) {
        auto pos = format.find("{}");
        if (pos != std::string_view::npos) {
            oss << format.substr(0, pos) << std::forward<T>(value);
            format_message_impl(oss, format.substr(pos + 2), std::forward<Args>(args)...);
        } else {
            oss << format;
        }
    }
    
    /**
     * @brief 获取当前时间戳字符串
     * 
     * 格式: YYYY-MM-DD HH:MM:SS.mmm
     * 
     * @return 时间戳字符串
     */
    std::string get_timestamp();
    
    LogLevel min_level_;                              // 最小日志级别
    std::vector<std::unique_ptr<LogOutput>> outputs_; // 输出目标列表
    mutable std::mutex mutex_;                        // 保护日志记录器状态
    
    // thread_local 协程 ID 存储
    static thread_local uint64_t current_coroutine_id_;
};

// 便捷宏定义

/**
 * @brief DEBUG 级别日志宏
 * 
 * @param module 模块名称
 * @param ... 消息和格式化参数
 * 
 * @example
 * LOG_DEBUG("Network", "Received {} bytes", bytes);
 */
#define LOG_DEBUG(module, ...) \
    ::frpc::Logger::instance().log(::frpc::LogLevel::DEBUG, module, __VA_ARGS__)

/**
 * @brief INFO 级别日志宏
 * 
 * @param module 模块名称
 * @param ... 消息和格式化参数
 * 
 * @example
 * LOG_INFO("RpcServer", "Server started on port {}", port);
 */
#define LOG_INFO(module, ...) \
    ::frpc::Logger::instance().log(::frpc::LogLevel::INFO, module, __VA_ARGS__)

/**
 * @brief WARN 级别日志宏
 * 
 * @param module 模块名称
 * @param ... 消息和格式化参数
 * 
 * @example
 * LOG_WARN("ConnectionPool", "Connection pool nearly full: {}/{}", active, max);
 */
#define LOG_WARN(module, ...) \
    ::frpc::Logger::instance().log(::frpc::LogLevel::WARN, module, __VA_ARGS__)

/**
 * @brief ERROR 级别日志宏
 * 
 * @param module 模块名称
 * @param ... 消息和格式化参数
 * 
 * @example
 * LOG_ERROR("RpcClient", "Call failed: {}", error_msg);
 */
#define LOG_ERROR(module, ...) \
    ::frpc::Logger::instance().log(::frpc::LogLevel::ERROR, module, __VA_ARGS__)

} // namespace frpc

#endif // FRPC_LOGGER_H
