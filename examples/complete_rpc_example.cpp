/**
 * @file complete_rpc_example.cpp
 * @brief Complete End-to-End FRPC MVP Example
 * 
 * This example demonstrates the complete RPC system working together:
 * - Server setup with multiple services
 * - Client making various RPC calls
 * - Error handling and success scenarios
 * - Real-world use case: User Management Service
 * 
 * This is a comprehensive example showing all MVP features in action.
 */

#include "frpc/rpc_server.h"
#include "frpc/rpc_client.h"
#include "frpc/serializer.h"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>

using namespace frpc;

// ============================================================================
// Helper Functions for Serialization
// ============================================================================

/**
 * @brief Serialize an integer (4 bytes, big-endian)
 */
ByteBuffer serialize_int(int32_t value) {
    ByteBuffer buffer(4);
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
    return buffer;
}

/**
 * @brief Deserialize an integer (4 bytes, big-endian)
 */
int32_t deserialize_int(const ByteBuffer& buffer) {
    if (buffer.size() < 4) return 0;
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/**
 * @brief Serialize a string (length-prefixed)
 */
ByteBuffer serialize_string(const std::string& str) {
    ByteBuffer buffer;
    auto len = serialize_int(static_cast<int32_t>(str.size()));
    buffer.insert(buffer.end(), len.begin(), len.end());
    buffer.insert(buffer.end(), str.begin(), str.end());
    return buffer;
}

/**
 * @brief Deserialize a string (length-prefixed)
 */
std::string deserialize_string(const ByteBuffer& buffer, size_t& offset) {
    if (offset + 4 > buffer.size()) return "";
    
    ByteBuffer len_bytes(buffer.begin() + offset, buffer.begin() + offset + 4);
    int32_t len = deserialize_int(len_bytes);
    offset += 4;
    
    if (offset + len > buffer.size()) return "";
    
    std::string result(buffer.begin() + offset, buffer.begin() + offset + len);
    offset += len;
    return result;
}

// ============================================================================
// User Management Service - Real-World Example
// ============================================================================

/**
 * @brief Simple in-memory user database
 */
class UserDatabase {
public:
    struct User {
        int32_t id;
        std::string name;
        std::string email;
        int32_t age;
    };
    
    void add_user(const User& user) {
        users_[user.id] = user;
    }
    
    bool get_user(int32_t id, User& user) const {
        auto it = users_.find(id);
        if (it != users_.end()) {
            user = it->second;
            return true;
        }
        return false;
    }
    
    std::vector<User> list_users() const {
        std::vector<User> result;
        for (const auto& pair : users_) {
            result.push_back(pair.second);
        }
        return result;
    }
    
    bool delete_user(int32_t id) {
        return users_.erase(id) > 0;
    }
    
    size_t count() const {
        return users_.size();
    }

private:
    std::unordered_map<int32_t, User> users_;
};

// Global user database (for this example)
UserDatabase g_user_db;

/**
 * @brief Serialize a User object
 */
ByteBuffer serialize_user(const UserDatabase::User& user) {
    ByteBuffer buffer;
    
    auto id_bytes = serialize_int(user.id);
    buffer.insert(buffer.end(), id_bytes.begin(), id_bytes.end());
    
    auto name_bytes = serialize_string(user.name);
    buffer.insert(buffer.end(), name_bytes.begin(), name_bytes.end());
    
    auto email_bytes = serialize_string(user.email);
    buffer.insert(buffer.end(), email_bytes.begin(), email_bytes.end());
    
    auto age_bytes = serialize_int(user.age);
    buffer.insert(buffer.end(), age_bytes.begin(), age_bytes.end());
    
    return buffer;
}

/**
 * @brief Deserialize a User object
 */
UserDatabase::User deserialize_user(const ByteBuffer& buffer) {
    UserDatabase::User user;
    size_t offset = 0;
    
    if (offset + 4 <= buffer.size()) {
        ByteBuffer id_bytes(buffer.begin(), buffer.begin() + 4);
        user.id = deserialize_int(id_bytes);
        offset += 4;
    }
    
    user.name = deserialize_string(buffer, offset);
    user.email = deserialize_string(buffer, offset);
    
    if (offset + 4 <= buffer.size()) {
        ByteBuffer age_bytes(buffer.begin() + offset, buffer.begin() + offset + 4);
        user.age = deserialize_int(age_bytes);
    }
    
    return user;
}

// ============================================================================
// Service Handlers
// ============================================================================

/**
 * @brief Handler: Create a new user
 * Payload format: [id(4)][name_len(4)][name][email_len(4)][email][age(4)]
 */
Response handle_create_user(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    try {
        auto user = deserialize_user(req.payload);
        g_user_db.add_user(user);
        
        std::cout << "[Server] Created user: " << user.name 
                  << " (ID: " << user.id << ")" << std::endl;
        
        resp.error_code = ErrorCode::Success;
        resp.payload = serialize_string("User created successfully");
    } catch (const std::exception& e) {
        resp.error_code = ErrorCode::InvalidArgument;
        resp.error_message = std::string("Failed to create user: ") + e.what();
    }
    
    return resp;
}

/**
 * @brief Handler: Get user by ID
 * Payload format: [id(4)]
 */
Response handle_get_user(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    if (req.payload.size() < 4) {
        resp.error_code = ErrorCode::InvalidArgument;
        resp.error_message = "Invalid payload: expected user ID";
        return resp;
    }
    
    int32_t user_id = deserialize_int(req.payload);
    UserDatabase::User user;
    
    if (g_user_db.get_user(user_id, user)) {
        std::cout << "[Server] Retrieved user: " << user.name 
                  << " (ID: " << user.id << ")" << std::endl;
        
        resp.error_code = ErrorCode::Success;
        resp.payload = serialize_user(user);
    } else {
        resp.error_code = ErrorCode::ServiceException;
        resp.error_message = "User not found";
    }
    
    return resp;
}

/**
 * @brief Handler: List all users
 */
Response handle_list_users(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    auto users = g_user_db.list_users();
    
    std::cout << "[Server] Listing " << users.size() << " users" << std::endl;
    
    // Serialize user count
    ByteBuffer buffer = serialize_int(static_cast<int32_t>(users.size()));
    
    // Serialize each user
    for (const auto& user : users) {
        auto user_bytes = serialize_user(user);
        buffer.insert(buffer.end(), user_bytes.begin(), user_bytes.end());
    }
    
    resp.error_code = ErrorCode::Success;
    resp.payload = buffer;
    
    return resp;
}

/**
 * @brief Handler: Delete user by ID
 * Payload format: [id(4)]
 */
Response handle_delete_user(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    if (req.payload.size() < 4) {
        resp.error_code = ErrorCode::InvalidArgument;
        resp.error_message = "Invalid payload: expected user ID";
        return resp;
    }
    
    int32_t user_id = deserialize_int(req.payload);
    
    if (g_user_db.delete_user(user_id)) {
        std::cout << "[Server] Deleted user ID: " << user_id << std::endl;
        resp.error_code = ErrorCode::Success;
        resp.payload = serialize_string("User deleted successfully");
    } else {
        resp.error_code = ErrorCode::ServiceException;
        resp.error_message = "User not found";
    }
    
    return resp;
}

/**
 * @brief Handler: Get user count
 */
Response handle_user_count(const Request& req) {
    Response resp;
    resp.request_id = req.request_id;
    
    int32_t count = static_cast<int32_t>(g_user_db.count());
    
    std::cout << "[Server] User count: " << count << std::endl;
    
    resp.error_code = ErrorCode::Success;
    resp.payload = serialize_int(count);
    
    return resp;
}

// ============================================================================
// Server Setup
// ============================================================================

/**
 * @brief Setup the RPC server with all services
 */
void setup_server(RpcServer& server) {
    std::cout << "Setting up User Management Service..." << std::endl;
    
    server.register_service("user.create", handle_create_user);
    server.register_service("user.get", handle_get_user);
    server.register_service("user.list", handle_list_users);
    server.register_service("user.delete", handle_delete_user);
    server.register_service("user.count", handle_user_count);
    
    std::cout << "Registered " << server.service_count() << " services\n" << std::endl;
}

// ============================================================================
// Client Examples
// ============================================================================

/**
 * @brief Example 1: Create users
 */
void example_create_users(RpcClient& client) {
    std::cout << "=== Example 1: Creating Users ===" << std::endl;
    
    // Create user 1
    UserDatabase::User user1{1, "Alice Johnson", "alice@example.com", 28};
    auto payload1 = serialize_user(user1);
    
    std::cout << "[Client] Creating user: " << user1.name << std::endl;
    auto resp1 = client.call("user.create", payload1);
    
    if (resp1 && resp1->error_code == ErrorCode::Success) {
        size_t offset = 0;
        std::string msg = deserialize_string(resp1->payload, offset);
        std::cout << "[Client] ✓ " << msg << std::endl;
    } else {
        std::cout << "[Client] ✗ Failed to create user" << std::endl;
    }
    
    // Create user 2
    UserDatabase::User user2{2, "Bob Smith", "bob@example.com", 35};
    auto payload2 = serialize_user(user2);
    
    std::cout << "[Client] Creating user: " << user2.name << std::endl;
    auto resp2 = client.call("user.create", payload2);
    
    if (resp2 && resp2->error_code == ErrorCode::Success) {
        size_t offset = 0;
        std::string msg = deserialize_string(resp2->payload, offset);
        std::cout << "[Client] ✓ " << msg << std::endl;
    }
    
    // Create user 3
    UserDatabase::User user3{3, "Carol Davis", "carol@example.com", 42};
    auto payload3 = serialize_user(user3);
    
    std::cout << "[Client] Creating user: " << user3.name << std::endl;
    auto resp3 = client.call("user.create", payload3);
    
    if (resp3 && resp3->error_code == ErrorCode::Success) {
        size_t offset = 0;
        std::string msg = deserialize_string(resp3->payload, offset);
        std::cout << "[Client] ✓ " << msg << "\n" << std::endl;
    }
}

/**
 * @brief Example 2: Get user by ID
 */
void example_get_user(RpcClient& client) {
    std::cout << "=== Example 2: Getting User by ID ===" << std::endl;
    
    int32_t user_id = 2;
    auto payload = serialize_int(user_id);
    
    std::cout << "[Client] Requesting user ID: " << user_id << std::endl;
    auto resp = client.call("user.get", payload);
    
    if (resp && resp->error_code == ErrorCode::Success) {
        auto user = deserialize_user(resp->payload);
        std::cout << "[Client] ✓ Found user:" << std::endl;
        std::cout << "  ID: " << user.id << std::endl;
        std::cout << "  Name: " << user.name << std::endl;
        std::cout << "  Email: " << user.email << std::endl;
        std::cout << "  Age: " << user.age << "\n" << std::endl;
    } else if (resp) {
        std::cout << "[Client] ✗ Error: " << resp->error_message << "\n" << std::endl;
    }
}

/**
 * @brief Example 3: List all users
 */
void example_list_users(RpcClient& client) {
    std::cout << "=== Example 3: Listing All Users ===" << std::endl;
    
    std::cout << "[Client] Requesting user list..." << std::endl;
    auto resp = client.call("user.list", {});
    
    if (resp && resp->error_code == ErrorCode::Success) {
        size_t offset = 0;
        
        // Read user count
        ByteBuffer count_bytes(resp->payload.begin(), resp->payload.begin() + 4);
        int32_t count = deserialize_int(count_bytes);
        offset += 4;
        
        std::cout << "[Client] ✓ Found " << count << " users:" << std::endl;
        
        // Read each user
        for (int i = 0; i < count; ++i) {
            ByteBuffer user_bytes(resp->payload.begin() + offset, resp->payload.end());
            auto user = deserialize_user(user_bytes);
            
            std::cout << "  " << (i + 1) << ". " << user.name 
                      << " (" << user.email << "), Age: " << user.age << std::endl;
            
            // Calculate offset for next user
            offset += 4; // id
            offset += 4 + user.name.size(); // name
            offset += 4 + user.email.size(); // email
            offset += 4; // age
        }
        std::cout << std::endl;
    }
}

/**
 * @brief Example 4: Get user count
 */
void example_user_count(RpcClient& client) {
    std::cout << "=== Example 4: Getting User Count ===" << std::endl;
    
    std::cout << "[Client] Requesting user count..." << std::endl;
    auto resp = client.call("user.count", {});
    
    if (resp && resp->error_code == ErrorCode::Success) {
        int32_t count = deserialize_int(resp->payload);
        std::cout << "[Client] ✓ Total users: " << count << "\n" << std::endl;
    }
}

/**
 * @brief Example 5: Delete a user
 */
void example_delete_user(RpcClient& client) {
    std::cout << "=== Example 5: Deleting a User ===" << std::endl;
    
    int32_t user_id = 2;
    auto payload = serialize_int(user_id);
    
    std::cout << "[Client] Deleting user ID: " << user_id << std::endl;
    auto resp = client.call("user.delete", payload);
    
    if (resp && resp->error_code == ErrorCode::Success) {
        size_t offset = 0;
        std::string msg = deserialize_string(resp->payload, offset);
        std::cout << "[Client] ✓ " << msg << "\n" << std::endl;
    } else if (resp) {
        std::cout << "[Client] ✗ Error: " << resp->error_message << "\n" << std::endl;
    }
}

/**
 * @brief Example 6: Error handling - Get non-existent user
 */
void example_error_handling(RpcClient& client) {
    std::cout << "=== Example 6: Error Handling ===" << std::endl;
    
    // Try to get a user that doesn't exist
    int32_t user_id = 999;
    auto payload = serialize_int(user_id);
    
    std::cout << "[Client] Requesting non-existent user ID: " << user_id << std::endl;
    auto resp = client.call("user.get", payload);
    
    if (resp && resp->error_code != ErrorCode::Success) {
        std::cout << "[Client] ✓ Correctly handled error:" << std::endl;
        std::cout << "  Error Code: " << static_cast<int>(resp->error_code) << std::endl;
        std::cout << "  Error Message: " << resp->error_message << std::endl;
    }
    
    // Try to call a non-existent service
    std::cout << "\n[Client] Calling non-existent service..." << std::endl;
    auto resp2 = client.call("user.invalid", {});
    
    if (resp2 && resp2->error_code != ErrorCode::Success) {
        std::cout << "[Client] ✓ Correctly handled error:" << std::endl;
        std::cout << "  Error Code: " << static_cast<int>(resp2->error_code) << std::endl;
        std::cout << "  Error Message: " << resp2->error_message << "\n" << std::endl;
    }
}

/**
 * @brief Example 7: Using metadata
 */
void example_with_metadata(RpcClient& client) {
    std::cout << "=== Example 7: Using Metadata ===" << std::endl;
    
    // Create metadata
    std::unordered_map<std::string, std::string> metadata;
    metadata["trace_id"] = "trace-abc-123";
    metadata["client_version"] = "1.0.0";
    metadata["request_source"] = "web_app";
    
    std::cout << "[Client] Calling with metadata:" << std::endl;
    std::cout << "  trace_id: " << metadata["trace_id"] << std::endl;
    std::cout << "  client_version: " << metadata["client_version"] << std::endl;
    
    auto resp = client.call_with_metadata("user.count", {}, metadata);
    
    if (resp && resp->error_code == ErrorCode::Success) {
        int32_t count = deserialize_int(resp->payload);
        std::cout << "[Client] ✓ Result: " << count << " users" << std::endl;
        std::cout << "[Client] Request ID: " << resp->request_id << "\n" << std::endl;
    }
}

// ============================================================================
// Main Function
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     FRPC Framework - Complete End-to-End Example          ║" << std::endl;
    std::cout << "║     User Management Service Demo                          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // Create and setup server
    RpcServer server;
    setup_server(server);
    
    // Create client with transport function that calls the server directly
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        return server.handle_raw_request(request);
    });
    
    std::cout << "Server and Client initialized. Running examples...\n" << std::endl;
    
    // Run all examples
    example_create_users(client);
    example_get_user(client);
    example_list_users(client);
    example_user_count(client);
    example_delete_user(client);
    
    // Verify deletion
    std::cout << "=== Verifying Deletion ===" << std::endl;
    auto resp = client.call("user.count", {});
    if (resp && resp->error_code == ErrorCode::Success) {
        int32_t count = deserialize_int(resp->payload);
        std::cout << "[Client] Users remaining after deletion: " << count << "\n" << std::endl;
    }
    
    example_error_handling(client);
    example_with_metadata(client);
    
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     All Examples Completed Successfully!                  ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    return 0;
}
