# FRPC 测试

# 测试辅助函数
function(add_frpc_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME} 
        PRIVATE 
        frpc 
        GTest::gtest 
        GTest::gtest_main
        rapidcheck
        rapidcheck_gtest
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# 基础测试
add_frpc_test(placeholder_test placeholder_test.cpp)

# Task 2.1: 错误处理系统验证测试
add_frpc_test(test_error_handling_verification test_error_handling_verification.cpp)

# Task 7.1: 数据模型测试
add_frpc_test(test_data_models test_data_models.cpp)

# Task 7.2: 二进制序列化器测试
add_frpc_test(test_serializer test_serializer.cpp)

# RPC Server tests
add_frpc_test(test_rpc_server test_rpc_server.cpp)

# Task 13.7: RPC 请求路由属性测试
add_frpc_test(test_rpc_server_properties test_rpc_server_properties.cpp)

# RPC Client tests
add_frpc_test(test_rpc_client test_rpc_client.cpp)

# Task 14.5: RPC 客户端端到端调用属性测试
add_frpc_test(test_rpc_client_properties test_rpc_client_properties.cpp)

# Task 14.6: RPC 调用超时处理属性测试
add_frpc_test(test_rpc_client_timeout test_rpc_client_timeout.cpp)

# Task 2.2: 错误码单元测试
add_frpc_test(test_error_codes test_error_codes.cpp)

# Task 2.4: 日志系统单元测试
add_frpc_test(test_logger test_logger.cpp)

# Task 3.2: 内存池属性测试
add_frpc_test(test_memory_pool test_memory_pool.cpp)

# Task 3.4: 缓冲区池单元测试
add_frpc_test(test_buffer_pool test_buffer_pool.cpp)

# Task 4.2: 网络引擎基础功能单元测试
add_frpc_test(test_network_engine test_network_engine.cpp)

# Task 4.5: 网络事件分发属性测试
add_frpc_test(test_network_engine_properties test_network_engine_properties.cpp)

# Task 5.2: RpcTask 协程返回类型 awaitable 接口测试
add_frpc_test(test_rpctask_awaitable test_rpctask_awaitable.cpp)

# Task 5.3: 协程调度器核心功能测试
add_frpc_test(test_coroutine_scheduler test_coroutine_scheduler.cpp)

# Task 8.4: 连接池单元测试
add_frpc_test(test_connection_pool test_connection_pool.cpp)

# Task 8.5-8.10: 连接池属性测试
add_frpc_test(test_connection_pool_properties test_connection_pool_properties.cpp)

# Task 9.3: 服务注册中心单元测试
add_frpc_test(test_service_registry test_service_registry.cpp)

# Task 9.4: 服务注册和查询属性测试
add_frpc_test(test_service_registry_properties test_service_registry_properties.cpp)

# Task 10.2: 负载均衡器单元测试
add_frpc_test(test_load_balancer test_load_balancer.cpp)

# Task 10.3-10.7: 负载均衡器属性测试
add_frpc_test(test_load_balancer_properties test_load_balancer_properties.cpp)

# Task 11.2: 健康检测器单元测试
add_frpc_test(test_health_checker test_health_checker.cpp)

# Task 11.3: 健康检测定期执行属性测试
add_frpc_test(test_health_checker_properties test_health_checker_properties.cpp)

# Task 13.1: 配置管理单元测试
add_frpc_test(test_config test_config.cpp)

# Task 13.5: 服务器统计信息收集测试
add_frpc_test(test_server_stats test_server_stats.cpp)

# Task 15.2: 端到端集成测试
add_frpc_test(test_integration_e2e test_integration_e2e.cpp)

# Task 15.2: 简化的端到端集成测试（避免 ConnectionPool 依赖）
add_frpc_test(test_integration_simple test_integration_simple.cpp)

# Task 15.3: 错误信息完整性属性测试
add_frpc_test(test_error_completeness_property test_error_completeness_property.cpp)

# Task 15.4: 异常日志记录属性测试
add_frpc_test(test_exception_logging_property test_exception_logging_property.cpp)
