/**
 * @file test_logger.cpp
 * @brief 日志系统单元测试
 * 
 * 测试日志级别过滤、日志格式正确性和多线程并发写日志。
 * 
 * 验证需求: 12.4, 13.4
 */

#include <gtest/gtest.h>
#include "frpc/logger.h"
#include <fstream>
#include <thread>
#include <vector>
#include <atomic>
#include <filesystem>

using namespace frpc;

/**
 * @brief 测试辅助类 - 捕获日志输出
 */
class CaptureOutput : public LogOutput {
public:
    void write(const std::string& message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.push_back(message);
    }
    
    void flush() override {
        // 不需要实现
    }
    
    std::vector<std::string> get_messages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.clear();
    }
    
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_.size();
    }

private:
    std::vector<std::string> messages_;
    mutable std::mutex mutex_;
};

/**
 * @brief 日志系统测试夹具
 */
class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前重置日志系统
        // 注意：由于 Logger 是单例，我们需要清理状态
        // 这里我们通过创建新的输出来隔离测试
    }
    
    void TearDown() override {
        // 清理测试文件
        if (std::filesystem::exists("test_log.txt")) {
            std::filesystem::remove("test_log.txt");
        }
    }
};

/**
 * @brief 测试日志级别过滤
 * 
 * 验证需求 12.4: 日志级别控制
 */
TEST_F(LoggerTest, LogLevelFiltering) {
    // 创建一个新的 Logger 实例用于测试
    // 由于 Logger 是单例，我们直接使用它并设置级别
    auto& logger = Logger::instance();
    
    // 设置为 WARN 级别
    logger.set_level(LogLevel::WARN);
    EXPECT_EQ(logger.get_level(), LogLevel::WARN);
    
    // 设置为 INFO 级别
    logger.set_level(LogLevel::INFO);
    EXPECT_EQ(logger.get_level(), LogLevel::INFO);
    
    // 设置为 DEBUG 级别
    logger.set_level(LogLevel::DEBUG);
    EXPECT_EQ(logger.get_level(), LogLevel::DEBUG);
    
    // 设置为 ERROR 级别
    logger.set_level(LogLevel::ERROR);
    EXPECT_EQ(logger.get_level(), LogLevel::ERROR);
}

/**
 * @brief 测试日志格式正确性
 * 
 * 验证需求 12.4: 日志格式包含时间戳、线程ID、协程ID、模块名
 */
TEST_F(LoggerTest, LogFormatCorrectness) {
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::DEBUG);
    
    // 写入日志到文件
    logger.add_file_output("test_log.txt");
    
    // 设置协程 ID
    Logger::set_coroutine_id(42);
    
    // 记录不同级别的日志
    logger.log(LogLevel::DEBUG, "TestModule", "Debug message");
    logger.log(LogLevel::INFO, "TestModule", "Info message");
    logger.log(LogLevel::WARN, "TestModule", "Warn message");
    logger.log(LogLevel::ERROR, "TestModule", "Error message");
    
    logger.flush();
    
    // 读取日志文件并验证格式
    std::ifstream file("test_log.txt");
    ASSERT_TRUE(file.is_open());
    
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    // 验证至少有 4 行日志
    ASSERT_GE(lines.size(), 4);
    
    // 验证每行日志的格式
    for (const auto& log_line : lines) {
        // 检查是否包含时间戳 [YYYY-MM-DD HH:MM:SS.mmm]
        EXPECT_TRUE(log_line.find("[20") != std::string::npos);
        
        // 检查是否包含线程 ID [Thread-...]
        EXPECT_TRUE(log_line.find("[Thread-") != std::string::npos);
        
        // 检查是否包含协程 ID [Coro-42]
        EXPECT_TRUE(log_line.find("[Coro-42]") != std::string::npos);
        
        // 检查是否包含模块名 [TestModule]
        EXPECT_TRUE(log_line.find("[TestModule]") != std::string::npos);
    }
    
    // 验证日志级别
    EXPECT_TRUE(lines[0].find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(lines[1].find("[INFO]") != std::string::npos);
    EXPECT_TRUE(lines[2].find("[WARN]") != std::string::npos);
    EXPECT_TRUE(lines[3].find("[ERROR]") != std::string::npos);
    
    // 验证消息内容
    EXPECT_TRUE(lines[0].find("Debug message") != std::string::npos);
    EXPECT_TRUE(lines[1].find("Info message") != std::string::npos);
    EXPECT_TRUE(lines[2].find("Warn message") != std::string::npos);
    EXPECT_TRUE(lines[3].find("Error message") != std::string::npos);
}

/**
 * @brief 测试无协程 ID 的日志格式
 */
TEST_F(LoggerTest, LogFormatWithoutCoroutineId) {
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::INFO);
    
    // 清除协程 ID
    Logger::set_coroutine_id(0);
    
    // 写入日志到文件
    logger.add_file_output("test_log.txt");
    logger.log(LogLevel::INFO, "TestModule", "Test message");
    logger.flush();
    
    // 读取日志文件
    std::ifstream file("test_log.txt");
    ASSERT_TRUE(file.is_open());
    
    std::string line;
    std::getline(file, line);
    file.close();
    
    // 验证包含 [Coro-None]
    EXPECT_TRUE(line.find("[Coro-None]") != std::string::npos);
}

/**
 * @brief 测试日志消息格式化
 */
TEST_F(LoggerTest, MessageFormatting) {
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::INFO);
    
    logger.add_file_output("test_log.txt");
    
    // 测试带参数的格式化
    logger.log(LogLevel::INFO, "TestModule", "Value: {}", 42);
    logger.log(LogLevel::INFO, "TestModule", "String: {}", "hello");
    logger.log(LogLevel::INFO, "TestModule", "Multiple: {} and {}", 1, 2);
    
    logger.flush();
    
    // 读取日志文件
    std::ifstream file("test_log.txt");
    ASSERT_TRUE(file.is_open());
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    // 验证格式化结果
    ASSERT_GE(lines.size(), 3);
    EXPECT_TRUE(lines[0].find("Value: 42") != std::string::npos);
    EXPECT_TRUE(lines[1].find("String: hello") != std::string::npos);
    EXPECT_TRUE(lines[2].find("Multiple: 1 and 2") != std::string::npos);
}

/**
 * @brief 测试多线程并发写日志
 * 
 * 验证需求 13.4: 线程安全
 */
TEST_F(LoggerTest, ConcurrentLogging) {
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::INFO);
    
    logger.add_file_output("test_log.txt");
    
    const int num_threads = 10;
    const int logs_per_thread = 100;
    std::atomic<int> completed_threads{0};
    
    std::vector<std::thread> threads;
    
    // 创建多个线程并发写日志
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&logger, i, logs_per_thread, &completed_threads]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                logger.log(LogLevel::INFO, "Thread" + std::to_string(i), 
                          "Message {}", j);
            }
            completed_threads++;
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有线程都完成了
    EXPECT_EQ(completed_threads, num_threads);
    
    logger.flush();
    
    // 读取日志文件并验证日志数量
    std::ifstream file("test_log.txt");
    ASSERT_TRUE(file.is_open());
    
    int line_count = 0;
    std::string line;
    while (std::getline(file, line)) {
        line_count++;
        // 验证每行日志格式正确
        EXPECT_TRUE(line.find("[INFO]") != std::string::npos);
        EXPECT_TRUE(line.find("[Thread") != std::string::npos);
    }
    file.close();
    
    // 验证日志数量正确（至少应该有 num_threads * logs_per_thread 条）
    EXPECT_GE(line_count, num_threads * logs_per_thread);
}

/**
 * @brief 测试日志级别过滤功能
 */
TEST_F(LoggerTest, LevelFilteringFunctionality) {
    auto& logger = Logger::instance();
    
    logger.add_file_output("test_log.txt");
    
    // 设置为 WARN 级别，只输出 WARN 和 ERROR
    logger.set_level(LogLevel::WARN);
    
    logger.log(LogLevel::DEBUG, "TestModule", "Debug - should not appear");
    logger.log(LogLevel::INFO, "TestModule", "Info - should not appear");
    logger.log(LogLevel::WARN, "TestModule", "Warn - should appear");
    logger.log(LogLevel::ERROR, "TestModule", "Error - should appear");
    
    logger.flush();
    
    // 读取日志文件
    std::ifstream file("test_log.txt");
    ASSERT_TRUE(file.is_open());
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    // 应该只有 2 条日志（WARN 和 ERROR）
    EXPECT_EQ(lines.size(), 2);
    
    if (lines.size() >= 2) {
        EXPECT_TRUE(lines[0].find("[WARN]") != std::string::npos);
        EXPECT_TRUE(lines[0].find("Warn - should appear") != std::string::npos);
        
        EXPECT_TRUE(lines[1].find("[ERROR]") != std::string::npos);
        EXPECT_TRUE(lines[1].find("Error - should appear") != std::string::npos);
    }
}

/**
 * @brief 测试控制台输出
 */
TEST_F(LoggerTest, ConsoleOutput) {
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::INFO);
    
    // 添加控制台输出（输出到 stderr）
    logger.add_console_output();
    
    // 记录日志（会输出到控制台，但我们无法直接捕获 stderr）
    // 这里只是确保不会崩溃
    logger.log(LogLevel::INFO, "TestModule", "Console test message");
    logger.flush();
    
    // 测试通过表示没有崩溃
    SUCCEED();
}

/**
 * @brief 测试便捷宏
 */
TEST_F(LoggerTest, ConvenienceMacros) {
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::DEBUG);
    
    logger.add_file_output("test_log.txt");
    
    // 使用便捷宏
    LOG_DEBUG("MacroTest", "Debug message");
    LOG_INFO("MacroTest", "Info message");
    LOG_WARN("MacroTest", "Warn message");
    LOG_ERROR("MacroTest", "Error message");
    
    // 带参数的宏
    LOG_INFO("MacroTest", "Value: {}", 42);
    LOG_ERROR("MacroTest", "Error code: {}", 500);
    
    logger.flush();
    
    // 读取日志文件
    std::ifstream file("test_log.txt");
    ASSERT_TRUE(file.is_open());
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    // 验证至少有 6 条日志
    ASSERT_GE(lines.size(), 6);
    
    // 验证宏生成的日志格式正确
    EXPECT_TRUE(lines[0].find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(lines[1].find("[INFO]") != std::string::npos);
    EXPECT_TRUE(lines[2].find("[WARN]") != std::string::npos);
    EXPECT_TRUE(lines[3].find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(lines[4].find("Value: 42") != std::string::npos);
    EXPECT_TRUE(lines[5].find("Error code: 500") != std::string::npos);
}

/**
 * @brief 测试协程 ID 设置和获取
 */
TEST_F(LoggerTest, CoroutineIdManagement) {
    // 初始协程 ID 应该是 0
    EXPECT_EQ(Logger::get_coroutine_id(), 0);
    
    // 设置协程 ID
    Logger::set_coroutine_id(123);
    EXPECT_EQ(Logger::get_coroutine_id(), 123);
    
    // 修改协程 ID
    Logger::set_coroutine_id(456);
    EXPECT_EQ(Logger::get_coroutine_id(), 456);
    
    // 清除协程 ID
    Logger::set_coroutine_id(0);
    EXPECT_EQ(Logger::get_coroutine_id(), 0);
}

/**
 * @brief 测试多线程环境下的协程 ID 隔离
 */
TEST_F(LoggerTest, CoroutineIdThreadLocal) {
    std::atomic<bool> thread1_ok{false};
    std::atomic<bool> thread2_ok{false};
    
    // 线程 1 设置协程 ID 为 100
    std::thread t1([&thread1_ok]() {
        Logger::set_coroutine_id(100);
        EXPECT_EQ(Logger::get_coroutine_id(), 100);
        thread1_ok = true;
    });
    
    // 线程 2 设置协程 ID 为 200
    std::thread t2([&thread2_ok]() {
        Logger::set_coroutine_id(200);
        EXPECT_EQ(Logger::get_coroutine_id(), 200);
        thread2_ok = true;
    });
    
    t1.join();
    t2.join();
    
    // 验证两个线程都成功设置了各自的协程 ID
    EXPECT_TRUE(thread1_ok);
    EXPECT_TRUE(thread2_ok);
    
    // 主线程的协程 ID 应该不受影响
    EXPECT_EQ(Logger::get_coroutine_id(), 0);
}

/**
 * @brief 测试文件输出错误处理
 */
TEST_F(LoggerTest, FileOutputErrorHandling) {
    // 尝试打开一个无效的文件路径
    EXPECT_THROW({
        FileOutput output("/invalid/path/that/does/not/exist/test.log");
    }, std::runtime_error);
}

/**
 * @brief 测试日志级别转换函数
 */
TEST_F(LoggerTest, LogLevelToString) {
    EXPECT_EQ(log_level_to_string(LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(log_level_to_string(LogLevel::INFO), "INFO");
    EXPECT_EQ(log_level_to_string(LogLevel::WARN), "WARN");
    EXPECT_EQ(log_level_to_string(LogLevel::ERROR), "ERROR");
}
