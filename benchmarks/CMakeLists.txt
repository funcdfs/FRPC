# Benchmarks CMakeLists.txt

# Google Benchmark should already be available from parent CMakeLists.txt

# Benchmark executables
add_executable(benchmark_serialization benchmark_serialization.cpp)
target_link_libraries(benchmark_serialization
    frpc
    benchmark::benchmark
    benchmark::benchmark_main
)

add_executable(benchmark_coroutine benchmark_coroutine.cpp)
target_link_libraries(benchmark_coroutine
    frpc
    benchmark::benchmark
    benchmark::benchmark_main
)

add_executable(benchmark_memory_pool benchmark_memory_pool.cpp)
target_link_libraries(benchmark_memory_pool
    frpc
    benchmark::benchmark
    benchmark::benchmark_main
)

add_executable(benchmark_network_io benchmark_network_io.cpp)
target_link_libraries(benchmark_network_io
    frpc
    benchmark::benchmark
    benchmark::benchmark_main
)

# Add all benchmarks to a custom target
add_custom_target(run_benchmarks
    COMMAND benchmark_serialization
    COMMAND benchmark_coroutine
    COMMAND benchmark_memory_pool
    COMMAND benchmark_network_io
    DEPENDS benchmark_serialization benchmark_coroutine benchmark_memory_pool benchmark_network_io
    COMMENT "Running all performance benchmarks"
)
