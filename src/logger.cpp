/**
 * @file logger.cpp
 * @brief 日志系统实现
 */

#include "frpc/logger.h"
#include <iostream>
#include <ctime>

namespace frpc {

// thread_local 协程 ID 存储
thread_local uint64_t Logger::current_coroutine_id_ = 0;

// ConsoleOutput 实现

void ConsoleOutput::write(const std::string& message) {
    std::cerr << message << std::endl;
}

void ConsoleOutput::flush() {
    std::cerr.flush();
}

// FileOutput 实现

FileOutput::FileOutput(const std::string& filename) 
    : file_(filename, std::ios::app) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
}

FileOutput::~FileOutput() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileOutput::write(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_ << message << std::endl;
}

void FileOutput::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.flush();
}

// Logger 实现

Logger::Logger() : min_level_(LogLevel::INFO) {
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

LogLevel Logger::get_level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return min_level_;
}

void Logger::add_console_output() {
    std::lock_guard<std::mutex> lock(mutex_);
    outputs_.push_back(std::make_unique<ConsoleOutput>());
}

void Logger::add_file_output(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    outputs_.push_back(std::make_unique<FileOutput>(filename));
}

void Logger::log(LogLevel level, std::string_view module, std::string_view message) {
    // 快速检查级别（无锁）
    if (level < min_level_) {
        return;
    }
    
    // 格式化日志行
    auto log_line = format_log_line(level, module, message);
    
    // 写入所有输出目标
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& output : outputs_) {
        output->write(log_line);
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& output : outputs_) {
        output->flush();
    }
}

void Logger::set_coroutine_id(uint64_t coro_id) {
    current_coroutine_id_ = coro_id;
}

uint64_t Logger::get_coroutine_id() {
    return current_coroutine_id_;
}

std::string Logger::format_log_line(LogLevel level, std::string_view module, 
                                   std::string_view message) {
    std::ostringstream oss;
    
    // [时间戳]
    oss << "[" << get_timestamp() << "] ";
    
    // [级别]
    oss << "[" << log_level_to_string(level) << "] ";
    
    // [线程ID]
    oss << "[Thread-" << std::this_thread::get_id() << "] ";
    
    // [协程ID]
    auto coro_id = get_coroutine_id();
    if (coro_id != 0) {
        oss << "[Coro-" << coro_id << "] ";
    } else {
        oss << "[Coro-None] ";
    }
    
    // [模块]
    oss << "[" << module << "] ";
    
    // 消息
    oss << message;
    
    return oss.str();
}

void Logger::format_message_impl(std::ostringstream& oss, std::string_view format) {
    oss << format;
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now_time_t);
#else
    localtime_r(&now_time_t, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    
    return oss.str();
}

} // namespace frpc
