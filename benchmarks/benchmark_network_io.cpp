/**
 * @file benchmark_network_io.cpp
 * @brief Performance benchmarks for network I/O operations
 * 
 * This benchmark measures the throughput and latency of network I/O operations
 * including send, receive, and connection establishment. These metrics are
 * critical for achieving the target QPS and latency requirements.
 * 
 * Performance targets:
 * - QPS > 50,000
 * - P99 latency < 10ms
 * 
 * Requirements: 11.5, 11.6, 15.1
 */

#include <benchmark/benchmark.h>
#include "frpc/network_engine.h"
#include "frpc/buffer_pool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <atomic>

using namespace frpc;

/**
 * @brief Helper function to create a non-blocking socket
 */
static int create_nonblocking_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    return fd;
}

/**
 * @brief Helper function to create a listening socket
 */
static int create_listening_socket(uint16_t port) {
    int fd = create_nonblocking_socket();
    if (fd < 0) return -1;
    
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    if (listen(fd, 128) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

/**
 * @brief Benchmark buffer pool allocation for network I/O
 * 
 * Measures the overhead of acquiring and releasing buffers from the buffer pool.
 * Fast buffer management is critical for high-throughput network I/O.
 */
static void BM_BufferPoolAllocation(benchmark::State& state) {
    const size_t buffer_size = state.range(0);
    BufferPool pool(buffer_size, 1000);
    
    for (auto _ : state) {
        auto buffer = pool.acquire();
        benchmark::DoNotOptimize(buffer);
        pool.release(std::move(buffer));
    }
    
    state.SetBytesProcessed(state.iterations() * buffer_size);
}

/**
 * @brief Benchmark socket creation and destruction
 * 
 * Measures the overhead of creating and closing sockets.
 * This is relevant for connection pool management.
 */
static void BM_SocketCreation(benchmark::State& state) {
    for (auto _ : state) {
        int fd = create_nonblocking_socket();
        benchmark::DoNotOptimize(fd);
        close(fd);
    }
}

/**
 * @brief Benchmark local socket send/receive throughput
 * 
 * Measures the throughput of sending and receiving data over local sockets.
 * This provides a baseline for network I/O performance.
 */
static void BM_LocalSocketThroughput(benchmark::State& state) {
    const size_t message_size = state.range(0);
    
    // Create a socket pair for local communication
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        state.SkipWithError("Failed to create socket pair");
        return;
    }
    
    std::vector<uint8_t> send_buffer(message_size, 0xAB);
    std::vector<uint8_t> recv_buffer(message_size);
    
    // Spawn receiver thread
    std::atomic<bool> running{true};
    std::thread receiver([&]() {
        while (running) {
            ssize_t n = recv(sockets[1], recv_buffer.data(), recv_buffer.size(), 0);
            if (n <= 0) break;
        }
    });
    
    // Benchmark sending
    for (auto _ : state) {
        ssize_t sent = send(sockets[0], send_buffer.data(), send_buffer.size(), 0);
        if (sent < 0) {
            state.SkipWithError("Send failed");
            break;
        }
        benchmark::DoNotOptimize(sent);
    }
    
    running = false;
    close(sockets[0]);
    close(sockets[1]);
    receiver.join();
    
    state.SetBytesProcessed(state.iterations() * message_size);
}

/**
 * @brief Benchmark epoll event registration overhead
 * 
 * Measures the time to register and unregister events with epoll.
 * This is critical for the network engine's event loop performance.
 */
static void BM_EpollRegistration(benchmark::State& state) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        state.SkipWithError("Failed to create epoll");
        return;
    }
    
    int fd = create_nonblocking_socket();
    
    for (auto _ : state) {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
        
        benchmark::DoNotOptimize(ev);
    }
    
    close(fd);
    close(epoll_fd);
}

/**
 * @brief Benchmark connection establishment latency
 * 
 * Measures the time to establish a TCP connection.
 * This affects the connection pool's performance when creating new connections.
 */
static void BM_ConnectionEstablishment(benchmark::State& state) {
    // Create listening socket
    int listen_fd = create_listening_socket(0);  // Random port
    if (listen_fd < 0) {
        state.SkipWithError("Failed to create listening socket");
        return;
    }
    
    // Get the actual port
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    getsockname(listen_fd, (sockaddr*)&addr, &addr_len);
    uint16_t port = ntohs(addr.sin_port);
    
    // Spawn acceptor thread
    std::atomic<bool> running{true};
    std::thread acceptor([&]() {
        while (running) {
            int client_fd = accept(listen_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                close(client_fd);
            }
        }
    });
    
    for (auto _ : state) {
        int fd = create_nonblocking_socket();
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        server_addr.sin_port = htons(port);
        
        connect(fd, (sockaddr*)&server_addr, sizeof(server_addr));
        
        // Wait for connection to complete
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(fd, &write_fds);
        
        timeval timeout{0, 100000};  // 100ms timeout
        select(fd + 1, nullptr, &write_fds, nullptr, &timeout);
        
        close(fd);
        benchmark::DoNotOptimize(fd);
    }
    
    running = false;
    close(listen_fd);
    acceptor.join();
}

/**
 * @brief Benchmark message send latency
 * 
 * Measures the latency of sending a single message.
 * This contributes to the overall RPC call latency.
 */
static void BM_MessageSendLatency(benchmark::State& state) {
    const size_t message_size = state.range(0);
    
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        state.SkipWithError("Failed to create socket pair");
        return;
    }
    
    std::vector<uint8_t> buffer(message_size, 0xAB);
    
    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        ssize_t sent = send(sockets[0], buffer.data(), buffer.size(), 0);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        state.SetIterationTime(elapsed.count() / 1e9);
        
        benchmark::DoNotOptimize(sent);
        
        // Drain the receive buffer
        std::vector<uint8_t> recv_buf(message_size);
        recv(sockets[1], recv_buf.data(), recv_buf.size(), 0);
    }
    
    close(sockets[0]);
    close(sockets[1]);
    
    state.SetBytesProcessed(state.iterations() * message_size);
}

/**
 * @brief Benchmark concurrent connection handling
 * 
 * Simulates handling multiple concurrent connections to measure scalability.
 * This is relevant for achieving the 10,000+ concurrent connection target.
 */
static void BM_ConcurrentConnections(benchmark::State& state) {
    const int num_connections = state.range(0);
    
    for (auto _ : state) {
        state.PauseTiming();
        
        std::vector<int> fds;
        fds.reserve(num_connections);
        
        // Create multiple socket pairs
        for (int i = 0; i < num_connections; ++i) {
            int sockets[2];
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0) {
                fds.push_back(sockets[0]);
                fds.push_back(sockets[1]);
            }
        }
        
        state.ResumeTiming();
        
        // Measure epoll handling of multiple connections
        int epoll_fd = epoll_create1(0);
        
        for (int fd : fds) {
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = fd;
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
        }
        
        benchmark::DoNotOptimize(epoll_fd);
        
        close(epoll_fd);
        
        state.PauseTiming();
        
        for (int fd : fds) {
            close(fd);
        }
        
        state.ResumeTiming();
    }
    
    state.SetItemsProcessed(state.iterations() * num_connections);
}

// Register benchmarks
BENCHMARK(BM_BufferPoolAllocation)->Range(1024, 64*1024);
BENCHMARK(BM_SocketCreation);
BENCHMARK(BM_LocalSocketThroughput)->Range(64, 64*1024);
BENCHMARK(BM_EpollRegistration);
BENCHMARK(BM_ConnectionEstablishment);
BENCHMARK(BM_MessageSendLatency)->Range(64, 4096)->UseManualTime();
BENCHMARK(BM_ConcurrentConnections)->Range(10, 1000);

BENCHMARK_MAIN();
