/**
 * @file types.h
 * @brief FRPC 框架基础类型定义
 * 
 * 定义框架中使用的基础类型、别名和常量。
 */

#ifndef FRPC_TYPES_H
#define FRPC_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <coroutine>

namespace frpc {

// 基础类型别名
using Byte = uint8_t;
using ByteBuffer = std::vector<Byte>;
using ByteSpan = std::span<const Byte>;

// 时间类型
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;

// 协程句柄类型
using CoroutineHandle = std::coroutine_handle<>;

// 事件回调类型
using EventCallback = std::function<void()>;

// 常量定义
namespace constants {
    constexpr uint32_t MAGIC_NUMBER = 0x46525043;  // "FRPC"
    constexpr uint32_t PROTOCOL_VERSION = 1;
    constexpr size_t DEFAULT_BUFFER_SIZE = 4096;
    constexpr size_t MAX_PAYLOAD_SIZE = 10 * 1024 * 1024;  // 10MB
    constexpr int DEFAULT_TIMEOUT_MS = 5000;
    constexpr int DEFAULT_MAX_CONNECTIONS = 100;
    constexpr int DEFAULT_MIN_CONNECTIONS = 1;
}

} // namespace frpc

#endif // FRPC_TYPES_H
