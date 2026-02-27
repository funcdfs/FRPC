/**
 * @file exceptions.h
 * @brief FRPC 框架异常类定义
 * 
 * 定义框架中使用的异常类层次结构，所有异常都携带错误码和描述信息。
 */

#ifndef FRPC_EXCEPTIONS_H
#define FRPC_EXCEPTIONS_H

#include "error_codes.h"
#include <stdexcept>
#include <string>

namespace frpc {

/**
 * @brief FRPC 异常基类
 * 
 * 所有 FRPC 框架的异常都继承自此类，携带错误码和错误消息。
 */
class FrpcException : public std::runtime_error {
public:
    /**
     * @brief 构造函数
     * 
     * @param code 错误码
     * @param message 错误描述信息
     */
    FrpcException(ErrorCode code, const std::string& message)
        : std::runtime_error(message), error_code_(code) {}
    
    /**
     * @brief 获取错误码
     * 
     * @return 错误码
     */
    ErrorCode error_code() const noexcept { return error_code_; }

private:
    ErrorCode error_code_;
};

/**
 * @brief 网络异常基类
 * 
 * 所有网络相关的异常都继承自此类。
 * 
 * @example
 * try {
 *     // 网络操作
 * } catch (const NetworkException& e) {
 *     std::cerr << "Network error: " << e.what() << std::endl;
 *     std::cerr << "Error code: " << static_cast<int>(e.error_code()) << std::endl;
 * }
 */
class NetworkException : public FrpcException {
public:
    using FrpcException::FrpcException;
};

/**
 * @brief 连接失败异常
 * 
 * 当无法建立到远程服务器的连接时抛出。
 */
class ConnectionFailedException : public NetworkException {
public:
    explicit ConnectionFailedException(const std::string& message)
        : NetworkException(ErrorCode::ConnectionFailed, message) {}
};

/**
 * @brief 连接关闭异常
 * 
 * 当连接被意外关闭时抛出。
 */
class ConnectionClosedException : public NetworkException {
public:
    explicit ConnectionClosedException(const std::string& message)
        : NetworkException(ErrorCode::ConnectionClosed, message) {}
};

/**
 * @brief 超时异常
 * 
 * 当操作超过指定的超时时间时抛出。
 */
class TimeoutException : public NetworkException {
public:
    explicit TimeoutException(const std::string& message)
        : NetworkException(ErrorCode::Timeout, message) {}
};

/**
 * @brief 发送失败异常
 * 
 * 当数据发送失败时抛出。
 */
class SendFailedException : public NetworkException {
public:
    explicit SendFailedException(const std::string& message)
        : NetworkException(ErrorCode::SendFailed, message) {}
};

/**
 * @brief 接收失败异常
 * 
 * 当数据接收失败时抛出。
 */
class RecvFailedException : public NetworkException {
public:
    explicit RecvFailedException(const std::string& message)
        : NetworkException(ErrorCode::RecvFailed, message) {}
};

/**
 * @brief 序列化异常
 * 
 * 当对象序列化失败时抛出。
 */
class SerializationException : public FrpcException {
public:
    explicit SerializationException(const std::string& message)
        : FrpcException(ErrorCode::SerializationError, message) {}
};

/**
 * @brief 反序列化异常
 * 
 * 当字节流反序列化失败时抛出。
 */
class DeserializationException : public FrpcException {
public:
    explicit DeserializationException(const std::string& message)
        : FrpcException(ErrorCode::DeserializationError, message) {}
};

/**
 * @brief 格式无效异常
 * 
 * 当数据格式不符合协议规范时抛出。
 */
class InvalidFormatException : public FrpcException {
public:
    explicit InvalidFormatException(const std::string& message)
        : FrpcException(ErrorCode::InvalidFormat, message) {}
};

/**
 * @brief 服务未找到异常
 * 
 * 当请求的服务名称不存在时抛出。
 */
class ServiceNotFoundException : public FrpcException {
public:
    explicit ServiceNotFoundException(const std::string& message)
        : FrpcException(ErrorCode::ServiceNotFound, message) {}
};

/**
 * @brief 服务不可用异常
 * 
 * 当服务暂时不可用时抛出。
 */
class ServiceUnavailableException : public FrpcException {
public:
    explicit ServiceUnavailableException(const std::string& message)
        : FrpcException(ErrorCode::ServiceUnavailable, message) {}
};

/**
 * @brief 无可用实例异常
 * 
 * 当没有健康的服务实例可用时抛出。
 */
class NoInstanceAvailableException : public FrpcException {
public:
    explicit NoInstanceAvailableException(const std::string& message)
        : FrpcException(ErrorCode::NoInstanceAvailable, message) {}
};

/**
 * @brief 服务执行异常
 * 
 * 当服务处理函数执行过程中发生异常时抛出。
 */
class ServiceException : public FrpcException {
public:
    explicit ServiceException(const std::string& message)
        : FrpcException(ErrorCode::ServiceException, message) {}
};

/**
 * @brief 参数无效异常
 * 
 * 当传入的参数不符合要求时抛出。
 */
class InvalidArgumentException : public FrpcException {
public:
    explicit InvalidArgumentException(const std::string& message)
        : FrpcException(ErrorCode::InvalidArgument, message) {}
};

/**
 * @brief 内部错误异常
 * 
 * 当发生未预期的内部错误时抛出。
 */
class InternalErrorException : public FrpcException {
public:
    explicit InternalErrorException(const std::string& message)
        : FrpcException(ErrorCode::InternalError, message) {}
};

/**
 * @brief 内存不足异常
 * 
 * 当内存分配失败时抛出。
 */
class OutOfMemoryException : public FrpcException {
public:
    explicit OutOfMemoryException(const std::string& message)
        : FrpcException(ErrorCode::OutOfMemory, message) {}
};

/**
 * @brief 资源耗尽异常
 * 
 * 当系统资源（如文件描述符、连接数）耗尽时抛出。
 */
class ResourceExhaustedException : public FrpcException {
public:
    explicit ResourceExhaustedException(const std::string& message)
        : FrpcException(ErrorCode::ResourceExhausted, message) {}
};

} // namespace frpc

#endif // FRPC_EXCEPTIONS_H
