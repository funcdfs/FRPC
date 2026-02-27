/**
 * @file test_error_codes.cpp
 * @brief 错误码体系单元测试
 * 
 * 测试所有错误码都有对应的描述信息，以及异常类正确携带错误码和消息。
 * 
 * 验证需求: 12.1
 */

#include <gtest/gtest.h>
#include "frpc/error_codes.h"
#include "frpc/exceptions.h"

using namespace frpc;

/**
 * @brief 测试所有错误码都有对应的描述信息
 * 
 * 验证需求 12.1: 错误信息应包含错误码和错误描述
 */
TEST(ErrorCodesTest, AllErrorCodesHaveDescriptions) {
    // 测试成功码
    EXPECT_EQ(get_error_message(ErrorCode::Success), "成功");
    
    // 测试网络错误
    EXPECT_EQ(get_error_message(ErrorCode::NetworkError), "通用网络错误");
    EXPECT_EQ(get_error_message(ErrorCode::ConnectionFailed), "连接失败");
    EXPECT_EQ(get_error_message(ErrorCode::ConnectionClosed), "连接关闭");
    EXPECT_EQ(get_error_message(ErrorCode::SendFailed), "发送失败");
    EXPECT_EQ(get_error_message(ErrorCode::RecvFailed), "接收失败");
    EXPECT_EQ(get_error_message(ErrorCode::Timeout), "超时");
    
    // 测试序列化错误
    EXPECT_EQ(get_error_message(ErrorCode::SerializationError), "序列化失败");
    EXPECT_EQ(get_error_message(ErrorCode::DeserializationError), "反序列化失败");
    EXPECT_EQ(get_error_message(ErrorCode::InvalidFormat), "格式无效");
    
    // 测试服务错误
    EXPECT_EQ(get_error_message(ErrorCode::ServiceNotFound), "服务未找到");
    EXPECT_EQ(get_error_message(ErrorCode::ServiceUnavailable), "服务不可用");
    EXPECT_EQ(get_error_message(ErrorCode::NoInstanceAvailable), "没有可用的服务实例");
    EXPECT_EQ(get_error_message(ErrorCode::ServiceException), "服务执行异常");
    
    // 测试参数错误
    EXPECT_EQ(get_error_message(ErrorCode::InvalidArgument), "参数无效");
    EXPECT_EQ(get_error_message(ErrorCode::MissingArgument), "缺少参数");
    
    // 测试系统错误
    EXPECT_EQ(get_error_message(ErrorCode::InternalError), "内部错误");
    EXPECT_EQ(get_error_message(ErrorCode::OutOfMemory), "内存不足");
    EXPECT_EQ(get_error_message(ErrorCode::ResourceExhausted), "资源耗尽");
}

/**
 * @brief 测试未知错误码返回默认描述
 */
TEST(ErrorCodesTest, UnknownErrorCodeReturnsDefault) {
    auto unknown_code = static_cast<ErrorCode>(9999);
    EXPECT_EQ(get_error_message(unknown_code), "未知错误");
}

/**
 * @brief 测试错误码辅助函数
 */
TEST(ErrorCodesTest, ErrorCodeHelperFunctions) {
    // 测试 is_success
    EXPECT_TRUE(is_success(ErrorCode::Success));
    EXPECT_FALSE(is_success(ErrorCode::NetworkError));
    
    // 测试 is_network_error
    EXPECT_TRUE(is_network_error(ErrorCode::NetworkError));
    EXPECT_TRUE(is_network_error(ErrorCode::ConnectionFailed));
    EXPECT_TRUE(is_network_error(ErrorCode::Timeout));
    EXPECT_FALSE(is_network_error(ErrorCode::SerializationError));
    EXPECT_FALSE(is_network_error(ErrorCode::ServiceNotFound));
    
    // 测试 is_serialization_error
    EXPECT_TRUE(is_serialization_error(ErrorCode::SerializationError));
    EXPECT_TRUE(is_serialization_error(ErrorCode::DeserializationError));
    EXPECT_TRUE(is_serialization_error(ErrorCode::InvalidFormat));
    EXPECT_FALSE(is_serialization_error(ErrorCode::NetworkError));
    EXPECT_FALSE(is_serialization_error(ErrorCode::ServiceNotFound));
    
    // 测试 is_service_error
    EXPECT_TRUE(is_service_error(ErrorCode::ServiceNotFound));
    EXPECT_TRUE(is_service_error(ErrorCode::ServiceUnavailable));
    EXPECT_TRUE(is_service_error(ErrorCode::NoInstanceAvailable));
    EXPECT_TRUE(is_service_error(ErrorCode::ServiceException));
    EXPECT_FALSE(is_service_error(ErrorCode::NetworkError));
    EXPECT_FALSE(is_service_error(ErrorCode::SerializationError));
}

/**
 * @brief 测试异常类正确携带错误码和消息
 * 
 * 验证需求 12.1: 异常应包含错误码和描述信息
 */
TEST(ExceptionsTest, ExceptionsCarryErrorCodeAndMessage) {
    // 测试 NetworkException
    try {
        throw ConnectionFailedException("无法连接到服务器");
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ConnectionFailed);
        EXPECT_STREQ(e.what(), "无法连接到服务器");
    }
    
    // 测试 TimeoutException
    try {
        throw TimeoutException("请求超时");
    } catch (const TimeoutException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::Timeout);
        EXPECT_STREQ(e.what(), "请求超时");
    }
    
    // 测试 SerializationException
    try {
        throw SerializationException("序列化对象失败");
    } catch (const SerializationException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::SerializationError);
        EXPECT_STREQ(e.what(), "序列化对象失败");
    }
    
    // 测试 DeserializationException
    try {
        throw DeserializationException("反序列化数据失败");
    } catch (const DeserializationException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::DeserializationError);
        EXPECT_STREQ(e.what(), "反序列化数据失败");
    }
    
    // 测试 ServiceNotFoundException
    try {
        throw ServiceNotFoundException("服务 'user_service' 未找到");
    } catch (const ServiceNotFoundException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ServiceNotFound);
        EXPECT_STREQ(e.what(), "服务 'user_service' 未找到");
    }
    
    // 测试 ServiceUnavailableException
    try {
        throw ServiceUnavailableException("服务暂时不可用");
    } catch (const ServiceUnavailableException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ServiceUnavailable);
        EXPECT_STREQ(e.what(), "服务暂时不可用");
    }
}

/**
 * @brief 测试异常继承层次结构
 */
TEST(ExceptionsTest, ExceptionHierarchy) {
    // NetworkException 继承自 FrpcException
    try {
        throw ConnectionFailedException("测试");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ConnectionFailed);
    }
    
    // 所有异常都继承自 std::runtime_error
    try {
        throw SerializationException("测试");
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "测试");
    }
}

/**
 * @brief 测试所有网络异常类
 */
TEST(ExceptionsTest, AllNetworkExceptions) {
    // ConnectionFailedException
    try {
        throw ConnectionFailedException("连接失败");
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ConnectionFailed);
    }
    
    // ConnectionClosedException
    try {
        throw ConnectionClosedException("连接关闭");
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ConnectionClosed);
    }
    
    // SendFailedException
    try {
        throw SendFailedException("发送失败");
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::SendFailed);
    }
    
    // RecvFailedException
    try {
        throw RecvFailedException("接收失败");
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::RecvFailed);
    }
}

/**
 * @brief 测试所有服务异常类
 */
TEST(ExceptionsTest, AllServiceExceptions) {
    // ServiceNotFoundException
    try {
        throw ServiceNotFoundException("服务未找到");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ServiceNotFound);
    }
    
    // ServiceUnavailableException
    try {
        throw ServiceUnavailableException("服务不可用");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ServiceUnavailable);
    }
    
    // NoInstanceAvailableException
    try {
        throw NoInstanceAvailableException("无可用实例");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::NoInstanceAvailable);
    }
    
    // ServiceException
    try {
        throw ServiceException("服务执行异常");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ServiceException);
    }
}

/**
 * @brief 测试其他异常类
 */
TEST(ExceptionsTest, OtherExceptions) {
    // InvalidFormatException
    try {
        throw InvalidFormatException("格式无效");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::InvalidFormat);
    }
    
    // InvalidArgumentException
    try {
        throw InvalidArgumentException("参数无效");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::InvalidArgument);
    }
    
    // InternalErrorException
    try {
        throw InternalErrorException("内部错误");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::InternalError);
    }
    
    // OutOfMemoryException
    try {
        throw OutOfMemoryException("内存不足");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::OutOfMemory);
    }
    
    // ResourceExhaustedException
    try {
        throw ResourceExhaustedException("资源耗尽");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ResourceExhausted);
    }
}
