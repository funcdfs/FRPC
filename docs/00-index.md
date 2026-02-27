# FRPC 框架文档

欢迎使用 FRPC (Fast RPC Framework) 框架！

## 文档导航

### 入门指南

1. [**介绍**](01-introduction.md) - 了解 FRPC 框架
   - 什么是 FRPC
   - 核心特性
   - 适用场景
   - 快速示例

2. [**快速开始**](02-quick-start.md) - 5 分钟上手
   - 安装和构建
   - 第一个 RPC 服务
   - 配置文件使用
   - 运行示例程序

### 核心文档

3. [**架构设计**](03-architecture.md) - 框架内部结构
   - 整体架构
   - 核心组件
   - RPC 调用流程
   - 协程模型
   - 线程模型

4. [**使用指南**](04-user-guide.md) - 详细使用说明
   - 核心概念
   - RPC 服务端
   - RPC 客户端
   - 服务治理
   - 协程编程
   - 错误处理
   - 性能优化
   - 扩展开发

### 参考手册

5. [**配置说明**](05-configuration.md) - 配置参数详解
   - 服务端配置
   - 客户端配置
   - 连接池配置
   - 健康检测配置
   - 日志配置
   - 性能调优建议

6. [**数据模型与序列化**](06-data-models.md) - 数据结构和协议
   - Request/Response 数据模型
   - 二进制序列化格式
   - 自定义序列化
   - 最佳实践

7. [**最佳实践**](07-best-practices.md) - 使用建议
   - 服务设计
   - 错误处理
   - 性能优化
   - 资源管理
   - 监控和运维
   - 安全性
   - 测试策略

8. [**故障排查**](08-troubleshooting.md) - 问题诊断和解决
   - 连接问题
   - 超时问题
   - 性能问题
   - 内存问题
   - 协程问题
   - 配置问题
   - 编译问题
   - 调试技巧

9. [**API 参考**](09-api-reference.md) - API 文档
   - 核心类概览
   - 配置类
   - 数据结构
   - 错误码和异常
   - 日志系统
   - 统计信息

## 示例代码

查看 `examples/` 目录获取完整的示例程序：

- `hello_frpc.cpp` - Hello World 示例
- `complete_rpc_example.cpp` - 完整的 RPC 示例
- `calculator_service_example.cpp` - 计算器服务
- `echo_service_example.cpp` - 回声服务
- `config_usage_example.cpp` - 配置文件使用
- `coroutine_usage_example.cpp` - 协程使用
- `data_models_example.cpp` - 数据模型示例
- `serializer_example.cpp` - 序列化示例
- `server_stats_example.cpp` - 统计信息示例

详细说明请参考 [examples/README.md](../examples/README.md)

## API 文档

使用 Doxygen 生成完整的 API 文档：

```bash
# 生成文档
./docs/generate_docs.sh

# 查看文档
open docs/html/index.html
```

## 快速链接

### 常见任务

- [创建第一个服务](02-quick-start.md#第一个-rpc-服务)
- [配置服务端](05-configuration.md#服务端配置)
- [配置客户端](05-configuration.md#客户端配置)
- [错误处理](04-user-guide.md#错误处理)
- [性能优化](04-user-guide.md#性能优化)
- [服务发现](04-user-guide.md#服务注册与发现)
- [负载均衡](04-user-guide.md#负载均衡)
- [健康检测](04-user-guide.md#健康检测)

### 常见问题

- [连接失败怎么办？](08-troubleshooting.md#问题-客户端无法连接到服务器)
- [请求超时怎么办？](08-troubleshooting.md#问题-请求频繁超时)
- [如何提高性能？](08-troubleshooting.md#问题-qps-低于预期)
- [如何检测内存泄漏？](08-troubleshooting.md#问题-内存泄漏)
- [如何调试协程？](08-troubleshooting.md#协程问题)

## 贡献文档

如果您想为文档做出贡献，请遵循以下准则：

1. 使用清晰、简洁的语言
2. 提供实际的代码示例
3. 包含必要的注意事项和警告
4. 保持文档与代码同步
5. 使用 Markdown 格式编写用户文档
6. 使用 Doxygen 格式编写 API 文档注释

## 获取帮助

- 查看[故障排查指南](08-troubleshooting.md)
- 搜索 GitHub Issues: https://github.com/your-org/frpc-framework/issues
- 提交新 Issue
- 查看 API 文档: `docs/html/index.html`

## 文档版本

当前文档版本对应 FRPC 框架版本：**1.0.0**

最后更新时间：2024-01-15
