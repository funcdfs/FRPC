# RPC Server MVP Documentation

## Overview

The RPC Server MVP is a simplified but functional implementation of the core RPC server logic for the FRPC framework. This MVP focuses on the essential RPC functionality without the complexity of network engines, coroutines, or connection management.

## Features

The MVP RPC Server provides:

1. **Service Registration** - Register handler functions by service name
2. **Request Routing** - Route incoming requests to the correct handler
3. **Request Deserialization** - Deserialize binary request data using BinarySerializer
4. **Response Serialization** - Serialize response data back to binary format
5. **Error Handling** - Gracefully handle deserialization failures and service not found errors

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      RpcServer                              │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  handle_raw_request(bytes)                           │  │
│  │    1. Deserialize request                            │  │
│  │    2. Route to handler                               │  │
│  │    3. Execute handler                                │  │
│  │    4. Serialize response                             │  │
│  │    5. Return response bytes                          │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Service Registry                                    │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │ "add"      -> handler_function                 │  │  │
│  │  │ "multiply" -> handler_function                 │  │  │
│  │  │ "echo"     -> handler_function                 │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Usage

### Basic Example

```cpp
#include "frpc/rpc_server.h"
#include "frpc/serializer.h"

using namespace frpc;

int main() {
    // Create server
    RpcServer server;
    BinarySerializer serializer;
    
    // Register a service
    server.register_service("echo", [](const Request& req) {
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.payload = req.payload;  // Echo the payload
        return resp;
    });
    
    // Create a request
    Request request;
    request.request_id = 1;
    request.service_name = "echo";
    request.payload = {1, 2, 3, 4, 5};
    
    // Serialize request
    auto request_bytes = serializer.serialize(request);
    
    // Handle request
    auto response_bytes = server.handle_raw_request(request_bytes);
    
    // Deserialize response
    auto response = serializer.deserialize_response(response_bytes);
    
    // Check result
    if (response.error_code == ErrorCode::Success) {
        std::cout << "Success!" << std::endl;
    }
    
    return 0;
}
```

### Calculator Service Example

See `examples/rpc_server_example.cpp` for a complete calculator service implementation with:
- Add service
- Multiply service
- Echo service
- Error handling demonstrations

## API Reference

### RpcServer Class

#### Constructor
```cpp
RpcServer();
```
Creates a new RPC server instance.

#### register_service
```cpp
bool register_service(const std::string& service_name, ServiceHandler handler);
```
Registers a service handler function.

**Parameters:**
- `service_name` - The name of the service
- `handler` - The handler function (takes Request, returns Response)

**Returns:** `true` if registration succeeded, `false` if service already exists

#### unregister_service
```cpp
bool unregister_service(const std::string& service_name);
```
Removes a registered service.

**Returns:** `true` if service was found and removed, `false` otherwise

#### has_service
```cpp
bool has_service(const std::string& service_name) const;
```
Checks if a service is registered.

**Returns:** `true` if service exists, `false` otherwise

#### handle_raw_request
```cpp
std::vector<uint8_t> handle_raw_request(const std::vector<uint8_t>& request_bytes);
```
Main entry point for processing requests. Handles the complete request/response cycle.

**Parameters:**
- `request_bytes` - Serialized request data

**Returns:** Serialized response data

**Error Handling:**
- Returns error response with `DeserializationError` if request cannot be deserialized
- Returns error response with `ServiceNotFound` if service doesn't exist
- Returns error response with `ServiceException` if handler throws

#### handle_request
```cpp
Response handle_request(const Request& request);
```
Handles a deserialized request.

**Parameters:**
- `request` - The deserialized request

**Returns:** Response from the handler

#### service_count
```cpp
size_t service_count() const;
```
Returns the number of registered services.

## Requirements Satisfied

This MVP implementation satisfies the following requirements from the design document:

- **4.1**: Service registration interface ✓
- **4.2**: Request deserialization (using BinarySerializer) ✓
- **4.3**: Service routing ✓
- **4.4**: Response serialization ✓
- **4.5**: Handle deserialization failures ✓
- **4.6**: Handle service not found errors ✓

## Testing

The RPC server includes comprehensive tests in `tests/test_rpc_server.cpp`:

- Service registration and unregistration
- Request deserialization
- Service routing to correct handlers
- Response serialization
- Deserialization failure handling
- Service not found error handling
- Service exception handling
- Edge cases (empty service name, large payloads, multiple services)

Run tests with:
```bash
cd build
ctest -R test_rpc_server -V
```

## Limitations (MVP)

This is a simplified MVP. The following features are NOT included:

- ❌ Network I/O (no sockets, no listening on ports)
- ❌ Coroutines (handlers are synchronous functions)
- ❌ Concurrent request handling
- ❌ Connection management
- ❌ Service discovery integration
- ❌ Health checking
- ❌ Load balancing
- ❌ Timeout handling
- ❌ Statistics collection

These features will be added in the full implementation.

## Next Steps

To evolve this MVP into a full RPC server:

1. **Add Network Engine** - Integrate with the NetworkEngine for async I/O
2. **Add Coroutines** - Convert handlers to coroutines for concurrent processing
3. **Add Connection Management** - Track client connections
4. **Add Service Registry** - Integrate with service discovery
5. **Add Statistics** - Collect metrics (QPS, latency, error rates)
6. **Add Configuration** - Support ServerConfig for customization

## Design Decisions

### Why Synchronous Handlers?

The MVP uses synchronous handler functions instead of coroutines to:
- Simplify the initial implementation
- Focus on core RPC logic (registration, routing, serialization)
- Provide a working foundation for testing
- Allow incremental addition of async features

### Why In-Memory Only?

The MVP doesn't include network I/O to:
- Avoid complexity of socket management
- Enable easy unit testing
- Focus on the RPC protocol logic
- Provide a clear separation of concerns

### Error Handling Strategy

The server uses a defensive error handling approach:
- All deserialization errors are caught and converted to error responses
- All handler exceptions are caught and converted to error responses
- Request IDs are preserved in error responses (when available)
- Descriptive error messages are provided

This ensures the server never crashes due to bad input or handler bugs.

## Performance Considerations

Since this is an MVP without network I/O or concurrency:
- Performance is limited by single-threaded execution
- No connection pooling or reuse
- No async I/O benefits

The full implementation will address these with:
- Coroutine-based concurrent request handling
- Non-blocking network I/O
- Connection pooling
- Efficient memory management

## Conclusion

This MVP RPC Server provides a solid foundation for the FRPC framework. It demonstrates the core RPC concepts and provides a working implementation that can be tested and extended. The clean separation of concerns makes it easy to add network I/O, coroutines, and other advanced features incrementally.
