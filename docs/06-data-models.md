# 数据模型与序列化

## 数据模型

FRPC 框架的数据模型定义了 RPC 通信中使用的核心数据结构。

### Request（请求对象）

#### 结构定义

```cpp
struct Request {
    uint32_t request_id;
    std::string service_name;
    ByteBuffer payload;
    Duration timeout;
    std::unordered_map<std::string, std::string> metadata;
    
    static uint32_t generate_id();
};
```

#### 字段说明

- **request_id**: 唯一标识一次 RPC 调用的请求 ID
- **service_name**: 要调用的远程服务的名称
- **payload**: 序列化后的参数数据
- **timeout**: 此次 RPC 调用的最大等待时间
- **metadata**: 存储额外的键值对信息（如追踪 ID、认证令牌等）

#### 使用示例

```cpp
// 创建请求
Request request;
request.request_id = Request::generate_id();
request.service_name = "user_service";
request.payload = serialize_args(user_id, action);
request.timeout = std::chrono::milliseconds(3000);
request.metadata["trace_id"] = "abc-123";
```

### Response（响应对象）

#### 结构定义

```cpp
struct Response {
    uint32_t request_id;
    ErrorCode error_code;
    std::string error_message;
    ByteBuffer payload;
    std::unordered_map<std::string, std::string> metadata;
};
```

#### 字段说明

- **request_id**: 对应的请求 ID
- **error_code**: 指示 RPC 调用的执行结果
- **error_message**: 人类可读的错误描述信息
- **payload**: 序列化后的返回值数据
- **metadata**: 存储额外的键值对信息

#### 使用示例

成功响应：
```cpp
Response response;
response.request_id = request.request_id;
response.error_code = ErrorCode::Success;
response.payload = serialize_result(result);
response.metadata["server_id"] = "server-1";
```

错误响应：
```cpp
Response response;
response.request_id = request.request_id;
response.error_code = ErrorCode::ServiceNotFound;
response.error_message = "Service 'unknown_service' not found";
```

## 序列化格式

FRPC 使用自定义的二进制协议进行序列化，格式紧凑，性能高效。

### 基本约定

- **字节序**: 所有多字节整数使用网络字节序（大端序）
- **字符串**: 长度前缀的 UTF-8 字符串（4 字节长度 + N 字节内容）
- **字节数组**: 长度前缀的字节数组（4 字节长度 + N 字节内容）

### 协议常量

```cpp
魔数 (MAGIC_NUMBER):     0x46525043  // "FRPC" in ASCII
协议版本 (VERSION):       0x00000001  // 版本 1
消息类型 - 请求:          0x00000000
消息类型 - 响应:          0x00000001
最大 Payload 大小:        10 MB
```

### 请求格式

```
+------------------+------------------+
| 字段             | 大小             |
+------------------+------------------+
| 魔数             | 4 字节           |
| 协议版本         | 4 字节           |
| 消息类型         | 4 字节           |
| 请求 ID          | 4 字节           |
| 服务名长度       | 4 字节           |
| 服务名           | N 字节           |
| Payload 长度     | 4 字节           |
| Payload 数据     | N 字节           |
| 超时时间         | 4 字节           |
| Metadata 条目数  | 4 字节           |
| Metadata 条目    | 变长             |
+------------------+------------------+
```

### 响应格式

```
+------------------+------------------+
| 字段             | 大小             |
+------------------+------------------+
| 魔数             | 4 字节           |
| 协议版本         | 4 字节           |
| 消息类型         | 4 字节           |
| 请求 ID          | 4 字节           |
| 错误码           | 4 字节           |
| 错误消息长度     | 4 字节           |
| 错误消息         | N 字节           |
| Payload 长度     | 4 字节           |
| Payload 数据     | N 字节           |
| Metadata 条目数  | 4 字节           |
| Metadata 条目    | 变长             |
+------------------+------------------+
```

### 序列化示例

```cpp
#include <frpc/serializer.h>

// 创建序列化器
Serializer serializer;

// 序列化请求
Request request;
request.request_id = 1;
request.service_name = "user_service";
request.payload = {0x01, 0x02, 0x03};

auto data = serializer.serialize(request);

// 反序列化请求
auto decoded_request = serializer.deserialize_request(data);
```

## 错误处理

### 反序列化错误

常见的反序列化错误：

1. **魔数不匹配**: `InvalidFormatException` - "Invalid magic number"
2. **版本不匹配**: `InvalidFormatException` - "Unsupported protocol version"
3. **数据长度不足**: `DeserializationException` - "Insufficient data to read ..."
4. **Payload 超限**: `DeserializationException` - "Payload size exceeds maximum limit"

### 错误处理示例

```cpp
try {
    auto request = serializer.deserialize_request(data);
    // 处理请求...
} catch (const InvalidFormatException& e) {
    LOG_ERROR("Invalid format: {}", e.what());
} catch (const DeserializationException& e) {
    LOG_ERROR("Deserialization failed: {}", e.what());
}
```

## 最佳实践

### 1. 请求 ID 生成

始终使用 `Request::generate_id()` 生成请求 ID：

```cpp
// ✓ 正确
request.request_id = Request::generate_id();

// ✗ 错误
request.request_id = 123;  // 可能导致 ID 冲突
```

### 2. 响应 ID 匹配

确保响应的 request_id 与请求的 request_id 完全一致：

```cpp
Response response;
response.request_id = request.request_id;  // 必须匹配
```

### 3. 错误处理

客户端应该先检查 error_code，再处理 payload：

```cpp
if (response.error_code == ErrorCode::Success) {
    // 解析 payload
    auto result = deserialize_result(response.payload);
} else {
    // 处理错误
    std::cerr << "Error: " << response.error_message << std::endl;
}
```

### 4. 元数据使用

使用 metadata 传递追踪和监控信息：

```cpp
// 请求元数据
request.metadata["trace_id"] = generate_trace_id();
request.metadata["client_version"] = "1.0.0";

// 响应元数据
response.metadata["server_id"] = get_server_id();
response.metadata["process_time_ms"] = std::to_string(elapsed_ms);
```

## 自定义序列化

可以实现 `Serializer` 接口来支持其他序列化格式：

```cpp
class JsonSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const Request& request) override {
        // 实现 JSON 序列化
        json j;
        j["request_id"] = request.request_id;
        j["service_name"] = request.service_name;
        // ...
        std::string str = j.dump();
        return std::vector<uint8_t>(str.begin(), str.end());
    }
    
    Request deserialize_request(std::span<const uint8_t> data) override {
        // 实现 JSON 反序列化
        std::string str(data.begin(), data.end());
        json j = json::parse(str);
        
        Request request;
        request.request_id = j["request_id"];
        request.service_name = j["service_name"];
        // ...
        return request;
    }
};
```

## 性能考虑

- 请求 ID 生成使用原子操作，开销极小（< 10ns）
- payload 使用 `std::vector<uint8_t>`，支持高效的内存管理
- metadata 使用 `std::unordered_map`，查找和插入操作为 O(1)
- 序列化/反序列化时间复杂度为 O(n)，其中 n 是数据大小

## 示例程序

完整的使用示例请参考：
- `examples/data_models_example.cpp` - 数据模型基本使用
- `examples/serializer_example.cpp` - 序列化示例
- `tests/test_data_models.cpp` - 单元测试示例
- `tests/test_serializer.cpp` - 序列化测试

## 下一步

- [最佳实践](07-best-practices.md) - 使用最佳实践
- [故障排查](08-troubleshooting.md) - 常见问题解决
- [API 参考](09-api-reference.md) - API 文档
