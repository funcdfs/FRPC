/**
 * @file error_codes.h
 * @brief FRPC 框架错误码定义
 * 
 * 定义框架中使用的标准错误码体系，便于快速定位和处理问题。
 * 错误码按照功能模块分层：
 * - 1000-1999: 网络层错误
 * - 2000-2999: 序列化层错误
 * - 3000-3999: 服务层错误
 * - 4000-4999: 参数验证错误
 * - 5000-5999: 系统内部错误
 */

#ifndef FRPC_ERROR_CODES_H
#define FRPC_ERROR_CODES_H

#include <cstdint>
#include <string_view>

namespace frpc {

/**
 * @brief 错误码枚举
 * 
 * 定义所有可能的错误类型，每个错误码都有对应的描述信息和处理建议。
 * 
 * 错误码分类及处理建议：
 * 
 * **网络错误 (1000-1999)**
 * - ConnectionFailed: 连接失败，建议检查网络连接和目标地址，可自动重试
 * - ConnectionClosed: 连接关闭，建议从连接池移除该连接，重新建立连接
 * - SendFailed: 发送失败，建议关闭连接并重试
 * - RecvFailed: 接收失败，建议关闭连接并重试
 * - Timeout: 超时，建议取消当前操作，可配置重试策略
 * 
 * **序列化错误 (2000-2999)**
 * - SerializationError: 序列化失败，建议检查对象数据完整性，记录详细日志
 * - DeserializationError: 反序列化失败，建议检查数据格式和协议版本，返回错误响应
 * - InvalidFormat: 格式无效，建议验证数据格式，可能是协议不匹配
 * 
 * **服务错误 (3000-3999)**
 * - ServiceNotFound: 服务未找到，建议检查服务名称拼写，确认服务已注册
 * - ServiceUnavailable: 服务不可用，建议触发服务发现更新，尝试其他实例
 * - NoInstanceAvailable: 无可用实例，建议等待健康检测恢复或返回降级响应
 * - ServiceException: 服务执行异常，建议记录详细错误日志，返回错误响应
 * 
 * **参数错误 (4000-4999)**
 * - InvalidArgument: 参数无效，建议验证参数格式和范围，返回明确的错误信息
 * - MissingArgument: 缺少参数，建议检查必需参数是否完整
 * 
 * **系统错误 (5000-5999)**
 * - InternalError: 内部错误，建议记录详细日志，联系技术支持
 * - OutOfMemory: 内存不足，建议释放资源，增加内存配置
 * - ResourceExhausted: 资源耗尽，建议检查资源限制配置，清理空闲资源
 */
enum class ErrorCode : uint32_t {
    // 成功
    Success = 0,
    
    // 网络错误 (1000-1999)
    NetworkError = 1000,              ///< 通用网络错误
    ConnectionFailed = 1001,          ///< 连接失败 - 建议检查网络连接和目标地址，可自动重试
    ConnectionClosed = 1002,          ///< 连接关闭 - 建议从连接池移除该连接，重新建立连接
    SendFailed = 1003,                ///< 发送失败 - 建议关闭连接并重试
    RecvFailed = 1004,                ///< 接收失败 - 建议关闭连接并重试
    Timeout = 1005,                   ///< 超时 - 建议取消当前操作，可配置重试策略
    
    // 序列化错误 (2000-2999)
    SerializationError = 2000,        ///< 序列化失败 - 建议检查对象数据完整性，记录详细日志
    DeserializationError = 2001,      ///< 反序列化失败 - 建议检查数据格式和协议版本，返回错误响应
    InvalidFormat = 2002,             ///< 格式无效 - 建议验证数据格式，可能是协议不匹配
    
    // 服务错误 (3000-3999)
    ServiceNotFound = 3000,           ///< 服务未找到 - 建议检查服务名称拼写，确认服务已注册
    ServiceUnavailable = 3001,        ///< 服务不可用 - 建议触发服务发现更新，尝试其他实例
    NoInstanceAvailable = 3002,       ///< 没有可用的服务实例 - 建议等待健康检测恢复或返回降级响应
    ServiceException = 3003,          ///< 服务执行异常 - 建议记录详细错误日志，返回错误响应
    
    // 参数错误 (4000-4999)
    InvalidArgument = 4000,           ///< 参数无效 - 建议验证参数格式和范围，返回明确的错误信息
    MissingArgument = 4001,           ///< 缺少参数 - 建议检查必需参数是否完整
    
    // 系统错误 (5000-5999)
    InternalError = 5000,             ///< 内部错误 - 建议记录详细日志，联系技术支持
    OutOfMemory = 5001,               ///< 内存不足 - 建议释放资源，增加内存配置
    ResourceExhausted = 5002,         ///< 资源耗尽 - 建议检查资源限制配置，清理空闲资源
};

/**
 * @brief 获取错误码的描述信息
 * 
 * @param code 错误码
 * @return 错误描述字符串
 * 
 * @example
 * auto msg = get_error_message(ErrorCode::ConnectionFailed);
 * // msg = "连接失败"
 */
std::string_view get_error_message(ErrorCode code);

/**
 * @brief 检查错误码是否表示成功
 * 
 * @param code 错误码
 * @return true 表示成功，false 表示失败
 */
inline bool is_success(ErrorCode code) {
    return code == ErrorCode::Success;
}

/**
 * @brief 检查错误码是否为网络错误
 * 
 * @param code 错误码
 * @return true 表示网络错误
 */
inline bool is_network_error(ErrorCode code) {
    auto value = static_cast<uint32_t>(code);
    return value >= 1000 && value < 2000;
}

/**
 * @brief 检查错误码是否为序列化错误
 * 
 * @param code 错误码
 * @return true 表示序列化错误
 */
inline bool is_serialization_error(ErrorCode code) {
    auto value = static_cast<uint32_t>(code);
    return value >= 2000 && value < 3000;
}

/**
 * @brief 检查错误码是否为服务错误
 * 
 * @param code 错误码
 * @return true 表示服务错误
 */
inline bool is_service_error(ErrorCode code) {
    auto value = static_cast<uint32_t>(code);
    return value >= 3000 && value < 4000;
}

} // namespace frpc

#endif // FRPC_ERROR_CODES_H
