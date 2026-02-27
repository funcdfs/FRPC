# Stress Tests CMakeLists.txt

# Stress test server executable
add_executable(stress_test_server stress_test_server.cpp)
target_link_libraries(stress_test_server
    Threads::Threads
)

# Install scripts
install(PROGRAMS
    run_basic_load.sh
    run_high_concurrency.sh
    run_sustained_load.sh
    run_peak_load.sh
    run_all_tests.sh
    DESTINATION ${CMAKE_BINARY_DIR}/stress_tests
)

# Create results directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/stress_tests/results)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/stress_tests/results/archive)
