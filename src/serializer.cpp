/**
 * @file serializer.cpp
 * @brief FRPC 框架二进制序列化器实现
 * 
 * 实现二进制序列化器的序列化和反序列化逻辑。
 */

#include "frpc/serializer.h"
#include "frpc/exceptions.h"
#include <cstring>
#include <arpa/inet.h>  // for htonl, ntohl

namespace frpc {

// ============================================================================
// 辅助方法：写入基本类型
// ============================================================================

void BinarySerializer::write_uint32(ByteBuffer& buffer, uint32_t value) {
    // 转换为网络字节序（大端序）
    uint32_t network_value = htonl(value);
    const Byte* bytes = reinterpret_cast<const Byte*>(&network_value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(uint32_t));
}

void BinarySerializer::write_string(ByteBuffer& buffer, const std::string& str) {
    // 写入字符串长度
    write_uint32(buffer, static_cast<uint32_t>(str.size()));
    // 写入字符串内容
    buffer.insert(buffer.end(), str.begin(), str.end());
}

void BinarySerializer::write_bytes(ByteBuffer& buffer, const ByteBuffer& bytes) {
    // 写入字节数组长度
    write_uint32(buffer, static_cast<uint32_t>(bytes.size()));
    // 写入字节数组内容
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

void BinarySerializer::write_metadata(ByteBuffer& buffer, 
                                      const std::unordered_map<std::string, std::string>& metadata) {
    // 写入元数据条目数
    write_uint32(buffer, static_cast<uint32_t>(metadata.size()));
    // 写入每个键值对
    for (const auto& [key, value] : metadata) {
        write_string(buffer, key);
        write_string(buffer, value);
    }
}

// ============================================================================
// 辅助方法：读取基本类型
// ============================================================================

uint32_t BinarySerializer::read_uint32(ByteSpan& data) {
    // 检查数据长度是否足够
    if (data.size() < sizeof(uint32_t)) {
        throw DeserializationException("Insufficient data to read uint32");
    }
    
    // 读取网络字节序的值
    uint32_t network_value;
    std::memcpy(&network_value, data.data(), sizeof(uint32_t));
    
    // 转换为主机字节序
    uint32_t value = ntohl(network_value);
    
    // 移动数据指针
    data = data.subspan(sizeof(uint32_t));
    
    return value;
}

std::string BinarySerializer::read_string(ByteSpan& data) {
    // 读取字符串长度
    uint32_t length = read_uint32(data);
    
    // 检查数据长度是否足够
    if (data.size() < length) {
        throw DeserializationException("Insufficient data to read string");
    }
    
    // 读取字符串内容
    std::string str(reinterpret_cast<const char*>(data.data()), length);
    
    // 移动数据指针
    data = data.subspan(length);
    
    return str;
}

ByteBuffer BinarySerializer::read_bytes(ByteSpan& data) {
    // 读取字节数组长度
    uint32_t length = read_uint32(data);
    
    // 检查数据长度是否足够
    if (data.size() < length) {
        throw DeserializationException("Insufficient data to read bytes");
    }
    
    // 检查长度是否超过最大限制
    if (length > constants::MAX_PAYLOAD_SIZE) {
        throw DeserializationException("Payload size exceeds maximum limit");
    }
    
    // 读取字节数组内容
    ByteBuffer bytes(data.begin(), data.begin() + length);
    
    // 移动数据指针
    data = data.subspan(length);
    
    return bytes;
}

std::unordered_map<std::string, std::string> BinarySerializer::read_metadata(ByteSpan& data) {
    // 读取元数据条目数
    uint32_t count = read_uint32(data);
    
    // 读取每个键值对
    std::unordered_map<std::string, std::string> metadata;
    for (uint32_t i = 0; i < count; ++i) {
        std::string key = read_string(data);
        std::string value = read_string(data);
        metadata[key] = value;
    }
    
    return metadata;
}

// ============================================================================
// 辅助方法：验证头部
// ============================================================================

void BinarySerializer::validate_header(ByteSpan& data, uint32_t expected_message_type) {
    // 检查最小头部长度（魔数 + 版本 + 消息类型）
    if (data.size() < 12) {
        throw DeserializationException("Data too short for header");
    }
    
    // 读取并验证魔数
    uint32_t magic = read_uint32(data);
    if (magic != constants::MAGIC_NUMBER) {
        throw InvalidFormatException("Invalid magic number");
    }
    
    // 读取并验证协议版本
    uint32_t version = read_uint32(data);
    if (version != constants::PROTOCOL_VERSION) {
        throw InvalidFormatException("Unsupported protocol version");
    }
    
    // 读取并验证消息类型
    uint32_t message_type = read_uint32(data);
    if (message_type != expected_message_type) {
        throw InvalidFormatException("Unexpected message type");
    }
}

// ============================================================================
// 序列化方法
// ============================================================================

ByteBuffer BinarySerializer::serialize(const Request& request) {
    ByteBuffer buffer;
    
    try {
        // 写入头部
        write_uint32(buffer, constants::MAGIC_NUMBER);
        write_uint32(buffer, constants::PROTOCOL_VERSION);
        write_uint32(buffer, MESSAGE_TYPE_REQUEST);
        
        // 写入请求 ID
        write_uint32(buffer, request.request_id);
        
        // 写入服务名称
        write_string(buffer, request.service_name);
        
        // 写入 payload
        write_bytes(buffer, request.payload);
        
        // 写入超时时间（转换为毫秒）
        write_uint32(buffer, static_cast<uint32_t>(request.timeout.count()));
        
        // 写入元数据
        write_metadata(buffer, request.metadata);
        
        return buffer;
    } catch (const std::exception& e) {
        throw SerializationException(std::string("Failed to serialize request: ") + e.what());
    }
}

ByteBuffer BinarySerializer::serialize(const Response& response) {
    ByteBuffer buffer;
    
    try {
        // 写入头部
        write_uint32(buffer, constants::MAGIC_NUMBER);
        write_uint32(buffer, constants::PROTOCOL_VERSION);
        write_uint32(buffer, MESSAGE_TYPE_RESPONSE);
        
        // 写入请求 ID
        write_uint32(buffer, response.request_id);
        
        // 写入错误码
        write_uint32(buffer, static_cast<uint32_t>(response.error_code));
        
        // 写入错误消息
        write_string(buffer, response.error_message);
        
        // 写入 payload
        write_bytes(buffer, response.payload);
        
        // 写入元数据
        write_metadata(buffer, response.metadata);
        
        return buffer;
    } catch (const std::exception& e) {
        throw SerializationException(std::string("Failed to serialize response: ") + e.what());
    }
}

// ============================================================================
// 反序列化方法
// ============================================================================

Request BinarySerializer::deserialize_request(ByteSpan data) {
    try {
        // 验证头部
        validate_header(data, MESSAGE_TYPE_REQUEST);
        
        Request request;
        
        // 读取请求 ID
        request.request_id = read_uint32(data);
        
        // 读取服务名称
        request.service_name = read_string(data);
        
        // 读取 payload
        request.payload = read_bytes(data);
        
        // 读取超时时间
        uint32_t timeout_ms = read_uint32(data);
        request.timeout = Duration(timeout_ms);
        
        // 读取元数据
        request.metadata = read_metadata(data);
        
        return request;
    } catch (const FrpcException&) {
        // 重新抛出 FRPC 异常
        throw;
    } catch (const std::exception& e) {
        throw DeserializationException(std::string("Failed to deserialize request: ") + e.what());
    }
}

Response BinarySerializer::deserialize_response(ByteSpan data) {
    try {
        // 验证头部
        validate_header(data, MESSAGE_TYPE_RESPONSE);
        
        Response response;
        
        // 读取请求 ID
        response.request_id = read_uint32(data);
        
        // 读取错误码
        uint32_t error_code_value = read_uint32(data);
        response.error_code = static_cast<ErrorCode>(error_code_value);
        
        // 读取错误消息
        response.error_message = read_string(data);
        
        // 读取 payload
        response.payload = read_bytes(data);
        
        // 读取元数据
        response.metadata = read_metadata(data);
        
        return response;
    } catch (const FrpcException&) {
        // 重新抛出 FRPC 异常
        throw;
    } catch (const std::exception& e) {
        throw DeserializationException(std::string("Failed to deserialize response: ") + e.what());
    }
}

} // namespace frpc
