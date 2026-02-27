# FRPC Framework - Build Instructions

## Prerequisites

- **C++ Compiler**: GCC 10+, Clang 12+, or MSVC 2019+ with C++20 support
- **CMake**: Version 3.20 or higher
- **Git**: For cloning dependencies (optional, CMake will download them)

## Quick Build

```bash
# Create build directory
mkdir -p build
cd build

# Configure the project
cmake ..

# Build everything
cmake --build .

# Run tests
ctest --output-on-failure

# Run examples
./examples/hello_frpc
./examples/complete_rpc_example
```

## Detailed Build Steps

### 1. Configure

```bash
mkdir -p build
cd build
cmake ..
```

This will:
- Check for C++20 compiler support
- Download dependencies (Google Test, RapidCheck, Google Benchmark)
- Generate build files

**Expected output:**
```
-- The CXX compiler identification is ...
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: ... - works
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Found Threads: TRUE
-- Google Test not found, downloading...
-- Google Benchmark not found, downloading...
-- RapidCheck not found, downloading...
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
```

### 2. Build

```bash
cmake --build .
```

Or use make directly:
```bash
make -j$(nproc)  # Linux/macOS
```

**Expected output:**
```
[ 10%] Building CXX object CMakeFiles/frpc.dir/src/error_codes.cpp.o
[ 20%] Building CXX object CMakeFiles/frpc.dir/src/data_models.cpp.o
[ 30%] Building CXX object CMakeFiles/frpc.dir/src/serializer.cpp.o
[ 40%] Building CXX object CMakeFiles/frpc.dir/src/rpc_server.cpp.o
[ 50%] Building CXX object CMakeFiles/frpc.dir/src/rpc_client.cpp.o
[ 60%] Linking CXX static library libfrpc.a
[ 60%] Built target frpc
[ 70%] Building CXX object tests/CMakeFiles/test_error_handling_verification.dir/test_error_handling_verification.cpp.o
...
[100%] Built target complete_rpc_example
```

### 3. Run Tests

```bash
ctest --output-on-failure
```

Or run individual tests:
```bash
./tests/test_error_handling_verification
./tests/test_data_models
./tests/test_serializer
./tests/test_rpc_server
./tests/test_rpc_client
```

**Expected output:**
```
Test project .../build
    Start 1: test_error_handling_verification
1/5 Test #1: test_error_handling_verification ...   Passed    0.01 sec
    Start 2: test_data_models
2/5 Test #2: test_data_models ...................   Passed    0.02 sec
    Start 3: test_serializer
3/5 Test #3: test_serializer ....................   Passed    0.01 sec
    Start 4: test_rpc_server
4/5 Test #4: test_rpc_server ....................   Passed    0.01 sec
    Start 5: test_rpc_client
5/5 Test #5: test_rpc_client ....................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 5
```

### 4. Run Examples

```bash
# Hello World - Simplest example
./examples/hello_frpc

# Complete User Management Service
./examples/complete_rpc_example

# Server-focused example
./examples/rpc_server_example

# Client-focused example
./examples/rpc_client_example

# Data models example
./examples/data_models_example

# Serializer example
./examples/serializer_example
```

## Build Configurations

### Debug Build (Default)

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

Features:
- Debug symbols included
- AddressSanitizer enabled (detects memory errors)
- No optimizations
- Assertions enabled

### Release Build

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

Features:
- Full optimizations (-O3)
- No debug symbols
- Assertions disabled
- Smaller binaries

### With Verbose Output

```bash
cmake --build . --verbose
```

Or:
```bash
make VERBOSE=1
```

## Generate Documentation

```bash
# Requires Doxygen to be installed
make docs

# Open documentation
open docs/html/index.html  # macOS
xdg-open docs/html/index.html  # Linux
```

## Troubleshooting

### CMake Version Too Old

**Error:**
```
CMake Error: CMake 3.20 or higher is required.
```

**Solution:**
- Install newer CMake from https://cmake.org/download/
- Or use package manager: `brew install cmake` (macOS), `apt install cmake` (Ubuntu)

### C++20 Not Supported

**Error:**
```
error: 'coroutine' file not found
```

**Solution:**
- Update your compiler:
  - GCC: `sudo apt install g++-10` or higher
  - Clang: `sudo apt install clang-12` or higher
  - MSVC: Update Visual Studio to 2019 or later

### Missing Dependencies

**Error:**
```
Could not find Google Test
```

**Solution:**
- CMake will automatically download dependencies
- If download fails, check internet connection
- Or install manually: `sudo apt install libgtest-dev`

### Build Fails with Errors

**Solution:**
1. Clean build directory: `rm -rf build/*`
2. Reconfigure: `cmake ..`
3. Rebuild: `cmake --build .`

### Tests Fail

**Solution:**
1. Check if all tests pass: `ctest --output-on-failure`
2. Run individual failing test for details
3. Check for memory errors: Build with Debug configuration

## Platform-Specific Notes

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential cmake g++-10 git

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### macOS

```bash
# Install dependencies
brew install cmake

# Build
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### Windows (Visual Studio)

```powershell
# Create build directory
mkdir build
cd build

# Configure (generates Visual Studio solution)
cmake .. -G "Visual Studio 16 2019"

# Build
cmake --build . --config Release

# Run tests
ctest -C Release --output-on-failure
```

## Build Targets

```bash
# Build everything
cmake --build .

# Build only library
cmake --build . --target frpc

# Build only tests
cmake --build . --target test_rpc_server

# Build only examples
cmake --build . --target hello_frpc

# Clean build
cmake --build . --target clean

# Generate documentation
cmake --build . --target docs
```

## Continuous Integration

Example GitHub Actions workflow:

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies
      run: |
        sudo apt update
        sudo apt install -y cmake g++-10
    
    - name: Configure
      run: |
        mkdir build
        cd build
        cmake ..
    
    - name: Build
      run: cmake --build build
    
    - name: Test
      run: |
        cd build
        ctest --output-on-failure
```

## Performance Tips

### Faster Builds

```bash
# Use Ninja instead of Make
cmake -G Ninja ..
ninja

# Parallel builds
cmake --build . -j$(nproc)

# Use ccache
export CC="ccache gcc"
export CXX="ccache g++"
cmake ..
```

### Smaller Binaries

```bash
# Release build with size optimization
cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
cmake --build .

# Strip symbols
strip examples/hello_frpc
```

## Next Steps

After successful build:

1. **Run examples** - See the framework in action
2. **Read documentation** - Understand the API
3. **Write your own service** - Use examples as templates
4. **Explore tests** - Learn from test cases

## Getting Help

If you encounter issues:

1. Check this document for solutions
2. Read [README_MVP.md](README_MVP.md) for usage guide
3. Look at example code for patterns
4. Check test cases for edge cases

---

**Happy Building! 🚀**
