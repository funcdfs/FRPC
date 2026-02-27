# FRPC 示例程序

# 示例辅助函数
function(add_frpc_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} ${ARGN})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE frpc)
endfunction()

# Hello World - Simplest example
add_frpc_example(hello_frpc hello_frpc.cpp)

# Complete RPC Example - Comprehensive end-to-end demo
add_frpc_example(complete_rpc_example complete_rpc_example.cpp)

# Task 7.1: 数据模型示例
add_frpc_example(data_models_example data_models_example.cpp)

# Task 7.2: 二进制序列化器示例
add_frpc_example(serializer_example serializer_example.cpp)

# RPC Server example
add_frpc_example(rpc_server_example rpc_server_example.cpp)

# RPC Client example
add_frpc_example(rpc_client_example rpc_client_example.cpp)

# 后续任务会添加更多示例
# add_frpc_example(simple_server simple_server.cpp)
# add_frpc_example(simple_client simple_client.cpp)

# Task 13.1: 配置管理示例
add_frpc_example(config_usage_example config_usage_example.cpp)

# Task 13.5: 服务器统计信息示例
add_frpc_example(server_stats_example server_stats_example.cpp)

# Task 15.1: 完整的集成示例 - 服务注册、发现、负载均衡、健康检测
add_frpc_example(integration_example integration_example.cpp)
