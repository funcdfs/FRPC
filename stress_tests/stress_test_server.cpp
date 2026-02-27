/**
 * @file stress_test_server.cpp
 * @brief Simple HTTP server for stress testing FRPC framework
 * 
 * This server provides a minimal HTTP endpoint for stress testing.
 * It responds to all requests with a simple JSON response to measure
 * the framework's performance under load.
 * 
 * Requirements: 11.1, 11.2, 11.3
 */

#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

// Performance metrics
std::atomic<uint64_t> total_requests{0};
std::atomic<uint64_t> total_errors{0};
std::atomic<uint64_t> active_connections{0};
std::atomic<bool> running{true};

// Simple HTTP response
const char* HTTP_RESPONSE = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 27\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"status\":\"ok\",\"id\":1}";

/**
 * @brief Set socket to non-blocking mode
 */
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * @brief Handle client connection
 */
void handle_client(int client_fd) {
    active_connections++;
    
    char buffer[4096];
    ssize_t bytes_read;
    
    while (running) {
        bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
        
        if (bytes_read > 0) {
            // Simple request handling - just send response
            ssize_t bytes_sent = send(client_fd, HTTP_RESPONSE, strlen(HTTP_RESPONSE), 0);
            
            if (bytes_sent > 0) {
                total_requests++;
            } else {
                total_errors++;
                break;
            }
        } else if (bytes_read == 0) {
            // Connection closed
            break;
        } else {
            // Error or would block
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                total_errors++;
                break;
            }
            // Would block - wait a bit
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    close(client_fd);
    active_connections--;
}

/**
 * @brief Print statistics periodically
 */
void print_stats() {
    auto start_time = std::chrono::steady_clock::now();
    uint64_t last_requests = 0;
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        uint64_t current_requests = total_requests.load();
        uint64_t requests_delta = current_requests - last_requests;
        double qps = requests_delta / 5.0;
        
        std::cout << "[" << elapsed << "s] "
                  << "QPS: " << static_cast<int>(qps) << " | "
                  << "Total: " << current_requests << " | "
                  << "Active: " << active_connections.load() << " | "
                  << "Errors: " << total_errors.load() << std::endl;
        
        last_requests = current_requests;
    }
}

/**
 * @brief Signal handler for graceful shutdown
 */
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

/**
 * @brief Main server function
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    std::cout << "=========================================" << std::endl;
    std::cout << "FRPC Stress Test Server" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    // Bind to port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port " << port << ": " << strerror(errno) << std::endl;
        close(listen_fd);
        return 1;
    }
    
    // Listen for connections
    if (listen(listen_fd, 1024) < 0) {
        std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
        close(listen_fd);
        return 1;
    }
    
    std::cout << "Server listening on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    
    // Start statistics thread
    std::thread stats_thread(print_stats);
    
    // Accept connections
    while (running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EINTR) {
                // Interrupted by signal
                continue;
            }
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
            continue;
        }
        
        // Handle client in a new thread
        // Note: In production, use a thread pool or async I/O
        std::thread client_thread(handle_client, client_fd);
        client_thread.detach();
    }
    
    // Cleanup
    close(listen_fd);
    stats_thread.join();
    
    // Print final statistics
    std::cout << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Final Statistics" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Total Requests: " << total_requests.load() << std::endl;
    std::cout << "Total Errors: " << total_errors.load() << std::endl;
    std::cout << "Active Connections: " << active_connections.load() << std::endl;
    std::cout << "=========================================" << std::endl;
    
    return 0;
}
