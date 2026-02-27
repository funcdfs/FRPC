/**
 * @file hello_frpc.cpp
 * @brief FRPC Framework - Hello World Example
 * 
 * The simplest possible FRPC example showing:
 * - Server creation and service registration
 * - Client creation and RPC call
 * - Basic request/response flow
 * 
 * This is your starting point for learning FRPC!
 */

#include "frpc/rpc_server.h"
#include "frpc/rpc_client.h"
#include <iostream>
#include <string>

using namespace frpc;

int main() {
    std::cout << "╔═══════════════════════════════════════╗" << std::endl;
    std::cout << "║  FRPC Framework - Hello World!       ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // Step 1: Create an RPC server
    std::cout << "1. Creating RPC server..." << std::endl;
    RpcServer server;
    
    // Step 2: Register a simple "greet" service
    std::cout << "2. Registering 'greet' service..." << std::endl;
    server.register_service("greet", [](const Request& req) {
        // Extract name from payload
        std::string name(req.payload.begin(), req.payload.end());
        
        // Create greeting
        std::string greeting = "Hello, " + name + "!";
        
        // Build response
        Response resp;
        resp.request_id = req.request_id;
        resp.error_code = ErrorCode::Success;
        resp.payload = ByteBuffer(greeting.begin(), greeting.end());
        
        std::cout << "   [Server] Greeting: " << greeting << std::endl;
        
        return resp;
    });
    
    // Step 3: Create an RPC client
    std::cout << "3. Creating RPC client..." << std::endl;
    RpcClient client([&server](const ByteBuffer& request) -> std::optional<ByteBuffer> {
        // In MVP, we simulate network by calling server directly
        return server.handle_raw_request(request);
    });
    
    // Step 4: Make an RPC call
    std::cout << "4. Making RPC call to 'greet' service..." << std::endl;
    
    std::string name = "World";
    ByteBuffer payload(name.begin(), name.end());
    
    auto response = client.call("greet", payload);
    
    // Step 5: Handle the response
    std::cout << "5. Processing response..." << std::endl;
    
    if (response && response->error_code == ErrorCode::Success) {
        std::string result(response->payload.begin(), response->payload.end());
        std::cout << "   [Client] Received: " << result << std::endl;
        std::cout << std::endl;
        std::cout << "✓ Success! Your first RPC call worked!" << std::endl;
    } else if (response) {
        std::cout << "   [Client] Error: " << response->error_message << std::endl;
    } else {
        std::cout << "   [Client] Network error occurred" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "╔═══════════════════════════════════════╗" << std::endl;
    std::cout << "║  Next: Try complete_rpc_example      ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════╝" << std::endl;
    
    return 0;
}
