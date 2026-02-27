/**
 * @file test_error_handling_verification.cpp
 * @brief 验证错误处理系统的完整性
 * 
 * 此测试验证 Task 2.1 的所有要求：
 * - ErrorCode 枚举定义完整
 * - get_error_message() 函数返回所有错误码的描述
 * - 异常类层次结构正确
 * - 文档注释完整
 */

#include "frpc/error_codes.h"
#include "frpc/exceptions.h"
#include <gtest/gtest.h>
#include <iostream>

using namespace frpc;

/**
 * @brief 测试所有错误码都有对应的描述信息
 * 
 * 验证需求 12.1: 错误码体系定义完整
 */
TEST(ErrorHandlingTest, AllErrorCodesHaveMessages) {
    // 测试成功码
    EXPECT_EQ(get_error_message(ErrorCode::Success), "成功");
    
    // 测试网络错误 (1000-1999)
    EXPECT_FALSE(get_error_message(ErrorCode::NetworkError).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::ConnectionFailed).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::ConnectionClosed).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::SendFailed).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::RecvFailed).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::Timeout).empty());
    
    // 测试序列化错误 (2000-2999)
    EXPECT_FALSE(get_error_message(ErrorCode::SerializationError).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::DeserializationError).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::InvalidFormat).empty());
    
    // 测试服务错误 (3000-3999)
    EXPECT_FALSE(get_error_message(ErrorCode::ServiceNotFound).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::ServiceUnavailable).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::NoInstanceAvailable).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::ServiceException).empty());
    
    // 测试参数错误 (4000-4999)
    EXPECT_FALSE(get_error_message(ErrorCode::InvalidArgument).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::MissingArgument).empty());
    
    // 测试系统错误 (5000-5999)
    EXPECT_FALSE(get_error_message(ErrorCode::InternalError).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::OutOfMemory).empty());
    EXPECT_FALSE(get_error_message(ErrorCode::ResourceExhausted).empty());
}

/**
 * @brief 测试错误码分类辅助函数
 * 
 * 验证需求 12.2: 标准错误码体系
 */
TEST(ErrorHandlingTest, ErrorCodeClassification) {
    // 测试成功检查
    EXPECT_TRUE(is_success(ErrorCode::Success));
    EXPECT_FALSE(is_success(ErrorCode::NetworkError));
    
    // 测试网络错误分类
    EXPECT_TRUE(is_network_error(ErrorCode::ConnectionFailed));
    EXPECT_TRUE(is_network_error(ErrorCode::Timeout));
    EXPECT_FALSE(is_network_error(ErrorCode::SerializationError));
    
    // 测试序列化错误分类
    EXPECT_TRUE(is_serialization_error(ErrorCode::SerializationError));
    EXPECT_TRUE(is_serialization_error(ErrorCode::DeserializationError));
    EXPECT_FALSE(is_serialization_error(ErrorCode::ServiceNotFound));
    
    // 测试服务错误分类
    EXPECT_TRUE(is_service_error(ErrorCode::ServiceNotFound));
    EXPECT_TRUE(is_service_error(ErrorCode::ServiceUnavailable));
    EXPECT_FALSE(is_service_error(ErrorCode::InvalidArgument));
}

/**
 * @brief 测试异常类正确携带错误码和消息
 * 
 * 验证需求 12.1: 异常类携带错误信息
 */
TEST(ErrorHandlingTest, ExceptionsCarryErrorCodeAndMessage) {
    // 测试网络异常
    try {
        throw ConnectionFailedException("无法连接到服务器");
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ConnectionFailed);
        EXPECT_STREQ(e.what(), "无法连接到服务器");
    }
    
    // 测试超时异常
    try {
        throw TimeoutException("请求超时");
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::Timeout);
        EXPECT_STREQ(e.what(), "请求超时");
    }
    
    // 测试序列化异常
    try {
        throw SerializationException("序列化对象失败");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::SerializationError);
        EXPECT_STREQ(e.what(), "序列化对象失败");
    }
    
    // 测试反序列化异常
    try {
        throw DeserializationException("数据格式错误");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::DeserializationError);
        EXPECT_STREQ(e.what(), "数据格式错误");
    }
    
    // 测试服务异常
    try {
        throw ServiceNotFoundException("服务 'user_service' 未找到");
    } catch (const FrpcException& e) {
        EXPECT_EQ(e.error_code(), ErrorCode::ServiceNotFound);
        EXPECT_STREQ(e.what(), "服务 'user_service' 未找到");
    }
}

/**
 * @brief 测试异常类层次结构
 * 
 * 验证需求 12.1: 异常类层次结构正确
 */
TEST(ErrorHandlingTest, ExceptionHierarchy) {
    // 测试网络异常继承自 FrpcException
    ConnectionFailedException conn_ex("test");
    EXPECT_NO_THROW(dynamic_cast<FrpcException&>(conn_ex));
    EXPECT_NO_THROW(dynamic_cast<NetworkException&>(conn_ex));
    
    // 测试序列化异常继承自 FrpcException
    SerializationException ser_ex("test");
    EXPECT_NO_THROW(dynamic_cast<FrpcException&>(ser_ex));
    
    // 测试服务异常继承自 FrpcException
    ServiceNotFoundException svc_ex("test");
    EXPECT_NO_THROW(dynamic_cast<FrpcException&>(svc_ex));
}

/**
 * @brief 测试所有网络异常类型
 */
TEST(ErrorHandlingTest, AllNetworkExceptions) {
    EXPECT_EQ(ConnectionFailedException("test").error_code(), ErrorCode::ConnectionFailed);
    EXPECT_EQ(ConnectionClosedException("test").error_code(), ErrorCode::ConnectionClosed);
    EXPECT_EQ(TimeoutException("test").error_code(), ErrorCode::Timeout);
    EXPECT_EQ(SendFailedException("test").error_code(), ErrorCode::SendFailed);
    EXPECT_EQ(RecvFailedException("test").error_code(), ErrorCode::RecvFailed);
}

/**
 * @brief 测试所有服务异常类型
 */
TEST(ErrorHandlingTest, AllServiceExceptions) {
    EXPECT_EQ(ServiceNotFoundException("test").error_code(), ErrorCode::ServiceNotFound);
    EXPECT_EQ(ServiceUnavailableException("test").error_code(), ErrorCode::ServiceUnavailable);
    EXPECT_EQ(NoInstanceAvailableException("test").error_code(), ErrorCode::NoInstanceAvailable);
    EXPECT_EQ(ServiceException("test").error_code(), ErrorCode::ServiceException);
}

/**
 * @brief 测试所有系统异常类型
 */
TEST(ErrorHandlingTest, AllSystemExceptions) {
    EXPECT_EQ(InvalidArgumentException("test").error_code(), ErrorCode::InvalidArgument);
    EXPECT_EQ(InternalErrorException("test").error_code(), ErrorCode::InternalError);
    EXPECT_EQ(OutOfMemoryException("test").error_code(), ErrorCode::OutOfMemory);
    EXPECT_EQ(ResourceExhaustedException("test").error_code(), ErrorCode::ResourceExhausted);
}

/**
 * @brief 打印所有错误码及其描述（用于验证文档完整性）
 */
TEST(ErrorHandlingTest, PrintAllErrorMessages) {
    std::cout << "\n=== FRPC 错误码体系 ===\n" << std::endl;
    
    std::cout << "成功码:" << std::endl;
    std::cout << "  0: " << get_error_message(ErrorCode::Success) << std::endl;
    
    std::cout << "\n网络错误 (1000-1999):" << std::endl;
    std::cout << "  1000: " << get_error_message(ErrorCode::NetworkError) << std::endl;
    std::cout << "  1001: " << get_error_message(ErrorCode::ConnectionFailed) << std::endl;
    std::cout << "  1002: " << get_error_message(ErrorCode::ConnectionClosed) << std::endl;
    std::cout << "  1003: " << get_error_message(ErrorCode::SendFailed) << std::endl;
    std::cout << "  1004: " << get_error_message(ErrorCode::RecvFailed) << std::endl;
    std::cout << "  1005: " << get_error_message(ErrorCode::Timeout) << std::endl;
    
    std::cout << "\n序列化错误 (2000-2999):" << std::endl;
    std::cout << "  2000: " << get_error_message(ErrorCode::SerializationError) << std::endl;
    std::cout << "  2001: " << get_error_message(ErrorCode::DeserializationError) << std::endl;
    std::cout << "  2002: " << get_error_message(ErrorCode::InvalidFormat) << std::endl;
    
    std::cout << "\n服务错误 (3000-3999):" << std::endl;
    std::cout << "  3000: " << get_error_message(ErrorCode::ServiceNotFound) << std::endl;
    std::cout << "  3001: " << get_error_message(ErrorCode::ServiceUnavailable) << std::endl;
    std::cout << "  3002: " << get_error_message(ErrorCode::NoInstanceAvailable) << std::endl;
    std::cout << "  3003: " << get_error_message(ErrorCode::ServiceException) << std::endl;
    
    std::cout << "\n参数错误 (4000-4999):" << std::endl;
    std::cout << "  4000: " << get_error_message(ErrorCode::InvalidArgument) << std::endl;
    std::cout << "  4001: " << get_error_message(ErrorCode::MissingArgument) << std::endl;
    
    std::cout << "\n系统错误 (5000-5999):" << std::endl;
    std::cout << "  5000: " << get_error_message(ErrorCode::InternalError) << std::endl;
    std::cout << "  5001: " << get_error_message(ErrorCode::OutOfMemory) << std::endl;
    std::cout << "  5002: " << get_error_message(ErrorCode::ResourceExhausted) << std::endl;
    
    std::cout << "\n======================\n" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
