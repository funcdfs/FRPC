/**
 * @file serializer.h
 * @brief FRPC 框架序列化器定义
 * 
 * 定义序列化器接口和二进制序列化器实现。
 * 序列化器负责将 Request 和 Response 对象转换为字节流，
 * 以及从字节流恢复对象。
 */

#ifndef FRPC_SERIALIZER_H
#define FRPC_SERIALIZER_H

#include "data_models.h"
#include "types.h"
#include <span>

namespace frpc {

/**
 * @brief 序列化器接口
 * 
 * 定义序列化和反序列化的抽象接口，支持可插拔的序列化协议。
 * 实现类可以提供不同的序列化格式（如二进制、JSON、Protobuf 等）。
 * 
 * @note 所有方法都应该是线程安全的
 */
class Serializer {
public:
    virtual ~Serializer() = default;
    
    /**
     * @brief 序列化请求对象
     * 
     * 将 Request 对象转换为字节流，用于网络传输。
     * 
     * @param request 请求对象
     * @return 序列化后的字节流
     * @throws SerializationException 如果序列化失败
     * 
     * @example
     * @code
     * Request request;
     * request.request_id = Request::generate_id();
     * request.service_name = "user_service";
     * auto bytes = serializer->serialize(request);
     * @endcode
     */
    virtual ByteBuffer serialize(const Request& request) = 0;
    
    /**
     * @brief 序列化响应对象
     * 
     * 将 Response 对象转换为字节流，用于网络传输。
     * 
     * @param response 响应对象
     * @return 序列化后的字节流
     * @throws SerializationException 如果序列化失败
     * 
     * @example
     * @code
     * Response response;
     * response.request_id = 123;
     * response.error_code = ErrorCode::Success;
     * auto bytes = serializer->serialize(response);
     * @endcode
     */
    virtual ByteBuffer serialize(const Response& response) = 0;
    
    /**
     * @brief 反序列化请求对象
     * 
     * 从字节流恢复 Request 对象。
     * 
     * @param data 字节流
     * @return 请求对象
     * @throws DeserializationException 如果格式无效
     * @throws InvalidFormatException 如果协议不匹配
     * 
     * @example
     * @code
     * auto request = serializer->deserialize_request(bytes);
     * std::cout << "Service: " << request.service_name << std::endl;
     * @endcode
     */
    virtual Request deserialize_request(ByteSpan data) = 0;
    
    /**
     * @brief 反序列化响应对象
     * 
     * 从字节流恢复 Response 对象。
     * 
     * @param data 字节流
     * @return 响应对象
     * @throws DeserializationException 如果格式无效
     * @throws InvalidFormatException 如果协议不匹配
     * 
     * @example
     * @code
     * auto response = serializer->deserialize_response(bytes);
     * if (response.error_code == ErrorCode::Success) {
     *     // 处理成功响应
     * }
     * @endcode
     */
    virtual Response deserialize_response(ByteSpan data) = 0;
};

/**
 * @brief 二进制序列化器（默认实现）
 * 
 * 使用自定义的二进制协议进行序列化，格式紧凑，性能高效。
 * 
 * ## 协议格式
 * 
 * ### 请求格式 (Request)
 * ```
 * [4字节: 魔数 0x46525043 "FRPC"]
 * [4字节: 协议版本]
 * [4字节: 消息类型 0=Request]
 * [4字节: 请求ID]
 * [4字节: 服务名长度]
 * [N字节: 服务名 UTF-8]
 * [4字节: Payload长度]
 * [N字节: Payload数据]
 * [4字节: 超时时间(毫秒)]
 * [4字节: Metadata条目数]
 * [对于每个Metadata条目:
 *   [4字节: Key长度]
 *   [N字节: Key UTF-8]
 *   [4字节: Value长度]
 *   [N字节: Value UTF-8]
 * ]
 * ```
 * 
 * ### 响应格式 (Response)
 * ```
 * [4字节: 魔数 0x46525043 "FRPC"]
 * [4字节: 协议版本]
 * [4字节: 消息类型 1=Response]
 * [4字节: 请求ID]
 * [4字节: 错误码]
 * [4字节: 错误消息长度]
 * [N字节: 错误消息 UTF-8]
 * [4字节: Payload长度]
 * [N字节: Payload数据]
 * [4字节: Metadata条目数]
 * [对于每个Metadata条目:
 *   [4字节: Key长度]
 *   [N字节: Key UTF-8]
 *   [4字节: Value长度]
 *   [N字节: Value UTF-8]
 * ]
 * ```
 * 
 * ## 字节序
 * 所有多字节整数使用网络字节序（大端序）。
 * 
 * ## 数据类型支持
 * - 基本数据类型：整数、浮点数、字符串、布尔值（通过 payload 字段）
 * - 复合数据类型：数组、结构体、映射（通过 payload 字段）
 * - 元数据：键值对映射（metadata 字段）
 * 
 * ## 错误处理
 * - 魔数不匹配：抛出 InvalidFormatException
 * - 版本不匹配：抛出 InvalidFormatException
 * - 数据长度不足：抛出 DeserializationException
 * - 字符串编码错误：抛出 DeserializationException
 * 
 * @note 线程安全：所有方法都是无状态的，可以安全地在多线程环境中使用
 * @note 性能：序列化和反序列化操作的时间复杂度为 O(n)，其中 n 是数据大小
 */
class BinarySerializer : public Serializer {
public:
    /**
     * @brief 序列化请求对象
     * 
     * @param request 请求对象
     * @return 序列化后的字节流
     * @throws SerializationException 如果序列化失败
     */
    ByteBuffer serialize(const Request& request) override;
    
    /**
     * @brief 序列化响应对象
     * 
     * @param response 响应对象
     * @return 序列化后的字节流
     * @throws SerializationException 如果序列化失败
     */
    ByteBuffer serialize(const Response& response) override;
    
    /**
     * @brief 反序列化请求对象
     * 
     * @param data 字节流
     * @return 请求对象
     * @throws DeserializationException 如果格式无效
     * @throws InvalidFormatException 如果协议不匹配
     */
    Request deserialize_request(ByteSpan data) override;
    
    /**
     * @brief 反序列化响应对象
     * 
     * @param data 字节流
     * @return 响应对象
     * @throws DeserializationException 如果格式无效
     * @throws InvalidFormatException 如果协议不匹配
     */
    Response deserialize_response(ByteSpan data) override;

private:
    // 消息类型常量
    static constexpr uint32_t MESSAGE_TYPE_REQUEST = 0;
    static constexpr uint32_t MESSAGE_TYPE_RESPONSE = 1;
    
    // 辅助方法：写入基本类型
    static void write_uint32(ByteBuffer& buffer, uint32_t value);
    static void write_string(ByteBuffer& buffer, const std::string& str);
    static void write_bytes(ByteBuffer& buffer, const ByteBuffer& bytes);
    static void write_metadata(ByteBuffer& buffer, 
                               const std::unordered_map<std::string, std::string>& metadata);
    
    // 辅助方法：读取基本类型
    static uint32_t read_uint32(ByteSpan& data);
    static std::string read_string(ByteSpan& data);
    static ByteBuffer read_bytes(ByteSpan& data);
    static std::unordered_map<std::string, std::string> read_metadata(ByteSpan& data);
    
    // 辅助方法：验证头部
    static void validate_header(ByteSpan& data, uint32_t expected_message_type);
};

} // namespace frpc

#endif // FRPC_SERIALIZER_H
