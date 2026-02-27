# FRPC Framework - MVP Getting Started Guide

Welcome to the FRPC (Fast RPC) Framework MVP! This is a minimal but functional RPC framework that demonstrates core RPC concepts without the complexity of production features.

## 🎯 What's Included in the MVP

The MVP includes the essential building blocks of an RPC system:

### Core Components

1. **Data Models** (`frpc/data_models.h`)
   - `Request` - RPC request object with service name, payload, metadata
   - `Response` - RPC response object with error handling and results
   - Unique request ID generation for request/response matching

2. **Serialization** (`frpc/serializer.h`)
   - Binary serialization protocol (compact and efficient)
   - Request/Response serialization and deserialization
   - Magic number and version validation
   - Metadata support

3. **RPC Server** (`frpc/rpc_server.h`)
   - Service registration by name
   - Request routing to appropriate handlers
   - Automatic error handling (service not found, deserialization errors)
   - Synchronous request processing

4. **RPC Client** (`frpc/rpc_client.h`)
   - Remote service invocation
   - Request serialization and response deserialization
   - Metadata support
   - Error handling (network errors, service errors)

5. **Error Handling** (`frpc/error_codes.h`, `frpc/exceptions.h`)
   - Comprehensive error codes
   - Exception hierarchy for different error types
   - Human-readable error messages

## 🚀 Quick Start

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.15 or higher

### Building the Project

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Run tests
ctest --output-on-failure
```

### Running Examples

```bash
# From the build directory

# 1. Hello World - Basic framework introduction
./examples/hello_frpc

# 2. Complete Example - Full user management service demo
./examples/complete_rpc_example

# 3. Server Example - Server-side features
./examples/rpc_server_example

# 4. Client Example - Client-side features
./examples/rpc_client_example
```

## 📖 Simple Example

Here's a minimal example to get you started:

```cpp
#include "frpc/rpc_server.h"
#include "frpc/rpc_client.h"

using namespace frpc;

int main() {
    // 1. Create server
    RpcServer server;
    
    // 2. Register a service
    server.register_service("greet", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        
        // Echo the input as greeting
        std::string name(req.payload.begin(), req.payload.end());
        std::string greeting = "Hello, " + name + "!";
        resp.payload = ByteBuffer(greeting.begin(), greeting.end());
        
        return resp;
    });
    
    // 3. Create client (with transport that calls server directly)
    RpcClient client([&server](const ByteBuffer& request) {
        return server.handle_raw_request(request);
    });
    
    // 4. Make RPC call
    std::string name = "World";
    ByteBuffer payload(name.begin(), name.end());
    
    auto response = client.call("greet", payload);
    
    // 5. Handle response
    if (response && response->error_code == ErrorCode::Success) {
        std::string result(response->payload.begin(), response->payload.end());
        std::cout << result << std::endl;  // Output: Hello, World!
    }
    
    return 0;
}
```

## 📚 Example Programs

### 1. `hello_frpc.cpp` - Hello World
The simplest possible example showing basic framework initialization and error handling.

**What it demonstrates:**
- Framework initialization
- Error codes and messages
- Exception handling

**Run it:**
```bash
./examples/hello_frpc
```

### 2. `complete_rpc_example.cpp` - Complete User Management Service
A comprehensive real-world example implementing a user management service.

**What it demonstrates:**
- Multiple related services (CRUD operations)
- Custom data serialization
- Error handling scenarios
- Metadata usage
- Complete client-server interaction

**Services implemented:**
- `user.create` - Create a new user
- `user.get` - Get user by ID
- `user.list` - List all users
- `user.delete` - Delete a user
- `user.count` - Get user count

**Run it:**
```bash
./examples/complete_rpc_example
```

### 3. `rpc_server_example.cpp` - Server Features
Focused example showing server-side capabilities.

**What it demonstrates:**
- Service registration
- Multiple service handlers
- Request routing
- Error responses
- Invalid request handling

**Run it:**
```bash
./examples/rpc_server_example
```

### 4. `rpc_client_example.cpp` - Client Features
Focused example showing client-side capabilities.

**What it demonstrates:**
- Basic RPC calls
- Calls with metadata
- Error handling
- Multiple sequential calls
- Request ID tracking

**Run it:**
```bash
./examples/rpc_client_example
```

## 🔧 How It Works

### Request Flow

```
Client                          Server
  |                               |
  | 1. Create Request             |
  |    (service name, payload)    |
  |                               |
  | 2. Serialize Request          |
  |    (to bytes)                 |
  |                               |
  | 3. Send via Transport ------> | 4. Receive bytes
  |                               |
  |                               | 5. Deserialize Request
  |                               |
  |                               | 6. Route to Handler
  |                               |
  |                               | 7. Execute Handler
  |                               |
  |                               | 8. Create Response
  |                               |
  | 11. Deserialize Response      | 9. Serialize Response
  |                               |
  | 10. Receive bytes <---------- | 10. Send via Transport
  |                               |
  | 12. Return to Caller          |
  |                               |
```

### Key Concepts

**Service Registration:**
```cpp
server.register_service("service_name", [](const Request& req) {
    // Your service logic here
    Response resp;
    resp.request_id = req.request_id;
    resp.error_code = ErrorCode::Success;
    resp.payload = /* your result */;
    return resp;
});
```

**Making RPC Calls:**
```cpp
ByteBuffer payload = /* serialize your arguments */;
auto response = client.call("service_name", payload);

if (response && response->error_code == ErrorCode::Success) {
    // Success - process response->payload
} else if (response) {
    // Service error - check response->error_message
} else {
    // Network/transport error
}
```

**Error Handling:**
- `ErrorCode::Success` - Call succeeded
- `ErrorCode::ServiceNotFound` - Service doesn't exist
- `ErrorCode::InvalidArgument` - Bad parameters
- `ErrorCode::ServiceException` - Service handler threw exception
- `ErrorCode::DeserializationError` - Invalid request format
- `std::nullopt` - Network/transport failure

## ⚠️ What's NOT in the MVP

The MVP focuses on core RPC logic. The following features are **not included** and will be in the full version:

### Not Implemented (Future Work)

❌ **Network I/O**
- No actual TCP/UDP sockets
- No network engine
- Transport is simulated via function callbacks

❌ **Asynchronous Operations**
- No coroutines (`co_await`)
- All operations are synchronous
- No concurrent request handling

❌ **Connection Management**
- No connection pooling
- No connection lifecycle management
- No reconnection logic

❌ **Service Discovery**
- No service registry
- No dynamic service lookup
- Services are registered manually

❌ **Load Balancing**
- No server selection
- No health checks
- No failover

❌ **Advanced Features**
- No timeout enforcement (timeout parameter is accepted but not used)
- No retry mechanisms
- No circuit breakers
- No rate limiting
- No authentication/authorization
- No compression
- No encryption/TLS

### Why These Limitations?

The MVP is designed to:
1. **Demonstrate core RPC concepts** without overwhelming complexity
2. **Provide a working foundation** that can be extended
3. **Enable learning** by focusing on essential components
4. **Facilitate testing** with simple, synchronous code

The full version will add production-ready features on top of this solid foundation.

## 🧪 Testing

The MVP includes comprehensive tests:

```bash
# Run all tests
ctest --output-on-failure

# Run specific test
./tests/test_rpc_server
./tests/test_rpc_client
./tests/test_serializer
```

## 📖 API Documentation

Generate API documentation with Doxygen:

```bash
doxygen Doxyfile
# Open docs/html/index.html in your browser
```

## 🎓 Learning Path

Recommended order to explore the codebase:

1. **Start with `hello_frpc.cpp`** - Understand basic concepts
2. **Read `data_models.h`** - Learn about Request/Response
3. **Read `serializer.h`** - Understand serialization
4. **Explore `rpc_server_example.cpp`** - See server-side code
5. **Explore `rpc_client_example.cpp`** - See client-side code
6. **Study `complete_rpc_example.cpp`** - See everything together
7. **Read the tests** - See edge cases and error handling

## 🤝 Contributing

This is an MVP/learning project. Contributions are welcome!

Areas for improvement:
- Additional examples
- Better documentation
- More test coverage
- Performance benchmarks

## 📄 License

[Your license here]

## 🔗 Resources

- **Design Documents**: See `docs/` directory for detailed design
- **API Reference**: Generate with Doxygen
- **Examples**: See `examples/` directory
- **Tests**: See `tests/` directory for usage patterns

---

**Ready to build something amazing?** Start with `./examples/hello_frpc` and work your way up to the complete example!
