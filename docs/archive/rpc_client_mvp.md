# RPC Client MVP Implementation

## Overview

This document describes the simplified MVP (Minimum Viable Product) implementation of the RPC Client for the FRPC framework.

## Goals

Create a minimal but functional RPC client that demonstrates the core RPC client logic without the complexity of network engines or coroutines.

## Features Implemented

### Core Functionality

1. **Request Creation** (Requirement 5.1)
   - Create RPC request objects with unique request IDs
   - Support service name and payload specification
   - Support optional metadata

2. **Request Serialization** (Requirement 5.2)
   - Serialize request objects to byte streams using BinarySerializer
   - Handle serialization errors gracefully

3. **Network Send** (Requirement 5.3 - Simulated)
   - Use a transport function to simulate network I/O
   - In MVP, this is a function object that can be connected to a local server
   - In full implementation, this will use NetworkEngine for async I/O

4. **Response Deserialization** (Requirement 5.4)
   - Deserialize response byte streams back to Response objects
   - Handle deserialization errors gracefully

5. **Return Result to Caller** (Requirement 5.5)
   - Return Response objects to the caller
   - Use std::optional to indicate success/failure

6. **Handle Network Errors** (Requirement 5.6)
   - Detect transport failures
   - Return empty optional on network errors

7. **Handle Deserialization Errors** (Requirement 5.7)
   - Catch DeserializationException
   - Return empty optional on deserialization errors

## Files Created

### Header File
- **Path**: `include/frpc/rpc_client.h`
- **Description**: RPC client class definition with comprehensive documentation
- **Key Classes**:
  - `RpcClient`: Main client class
  - `TransportFunction`: Type alias for transport function

### Implementation File
- **Path**: `src/rpc_client.cpp`
- **Description**: RPC client implementation
- **Key Methods**:
  - `call()`: Basic RPC call
  - `call_with_metadata()`: RPC call with metadata
  - `create_request()`: Request object creation
  - `do_call()`: Core call logic

### Test File
- **Path**: `tests/test_rpc_client.cpp`
- **Description**: Comprehensive unit tests for RPC client
- **Test Cases**:
  - Basic RPC call
  - Call non-existent service
  - Service returns error
  - Call with metadata
  - Request ID generation
  - Network transport failure
  - Response deserialization failure
  - Empty payload
  - Large payload
  - Multiple calls
  - Call different services

### Example File
- **Path**: `examples/rpc_client_example.cpp`
- **Description**: Usage examples demonstrating various scenarios
- **Examples**:
  1. Basic RPC call (echo service)
  2. Computation service (add two numbers)
  3. Call with metadata (greet service)
  4. Error handling (non-existent service, error service)
  5. Multiple calls

## API Usage

### Basic Usage

```cpp
// Create a client with a transport function
RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
    return server.handle_raw_request(request);
});

// Call a service
ByteBuffer payload = serialize_args(1, 2);
auto response = client.call("add", payload);

// Handle response
if (response && response->error_code == ErrorCode::Success) {
    auto result = deserialize_result(response->payload);
    std::cout << "Result: " << result << std::endl;
} else if (response) {
    std::cerr << "Error: " << response->error_message << std::endl;
} else {
    std::cerr << "Network error" << std::endl;
}
```

### Call with Metadata

```cpp
std::unordered_map<std::string, std::string> metadata;
metadata["trace_id"] = "abc-123";
metadata["user_id"] = "42";

auto response = client.call_with_metadata("my_service", payload, metadata);
```

## Design Decisions

### Simplifications for MVP

1. **No Network I/O**: Uses a transport function instead of actual socket operations
   - Allows testing without network setup
   - Easy to integrate with local server for testing
   - Full implementation will use NetworkEngine

2. **No Coroutines**: Uses synchronous calls instead of co_await
   - Simpler implementation
   - Easier to understand and test
   - Full implementation will use C++20 coroutines

3. **No Connection Pool**: Each call is independent
   - No connection management overhead
   - Full implementation will use ConnectionPool for connection reuse

4. **No Service Discovery**: Direct service name specification
   - No ServiceRegistry integration
   - Full implementation will query ServiceRegistry

5. **No Load Balancing**: Single target per call
   - No LoadBalancer integration
   - Full implementation will select from multiple instances

6. **No Timeout Control**: Timeout parameter accepted but not enforced
   - MVP focuses on core logic
   - Full implementation will use timers for timeout

7. **No Retry Mechanism**: Single attempt per call
   - Simpler error handling
   - Full implementation will support configurable retries

### Error Handling Strategy

The client uses a layered error handling approach:

1. **Transport Layer**: Returns empty optional on network failure
2. **Serialization Layer**: Catches exceptions and returns empty optional
3. **Service Layer**: Returns Response with error code and message

This allows callers to distinguish between:
- Network/transport failures (empty optional)
- Service-level errors (Response with error code)

## Testing Strategy

### Unit Tests

The test suite covers:
- Happy path scenarios (successful calls)
- Error scenarios (service not found, service errors)
- Edge cases (empty payload, large payload)
- Multiple calls and different services
- Network and deserialization failures

### Integration Testing

The tests use a real RpcServer instance connected via the transport function, providing end-to-end validation of the client-server interaction.

## Requirements Satisfied

| Requirement | Description | Status |
|-------------|-------------|--------|
| 5.1 | Remote service call interface | ✅ Implemented |
| 5.2 | Request serialization | ✅ Implemented |
| 5.3 | Network send | ✅ Simulated |
| 5.4 | Response deserialization | ✅ Implemented |
| 5.5 | Return result to caller | ✅ Implemented |
| 5.6 | Handle network errors | ✅ Implemented |
| 5.7 | Handle deserialization errors | ✅ Implemented |

## Future Enhancements

For the full implementation, the following features should be added:

1. **Async I/O**: Integrate with NetworkEngine for non-blocking operations
2. **Coroutines**: Use C++20 coroutines for async/await syntax
3. **Connection Pool**: Reuse connections to reduce overhead
4. **Service Discovery**: Query ServiceRegistry for available instances
5. **Load Balancing**: Select instances using LoadBalancer strategies
6. **Timeout Control**: Implement actual timeout mechanism with timers
7. **Retry Logic**: Automatic retry on transient failures
8. **Circuit Breaker**: Prevent cascading failures
9. **Metrics Collection**: Track call statistics (latency, success rate, etc.)
10. **Distributed Tracing**: Integrate with tracing systems

## Building and Running

### Build

```bash
mkdir -p build
cd build
cmake ..
make
```

### Run Tests

```bash
./bin/test_rpc_client
```

### Run Example

```bash
./bin/rpc_client_example
```

## Conclusion

This MVP implementation provides a solid foundation for the RPC client, demonstrating all core functionality in a simple, testable manner. It can be used immediately for testing and development, while the architecture allows for incremental enhancement toward the full implementation with async I/O, coroutines, and advanced features.
