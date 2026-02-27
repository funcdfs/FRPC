# FRPC 框架文档

本目录包含 FRPC 框架的完整文档。

## 文档结构

### 核心文档（按阅读顺序）

1. [**文档索引**](00-index.md) - 文档导航和快速链接
2. [**介绍**](01-introduction.md) - 框架概述和核心特性
3. [**快速开始**](02-quick-start.md) - 5 分钟快速上手指南
4. [**架构设计**](03-architecture.md) - 框架架构和设计原理
5. [**使用指南**](04-user-guide.md) - 详细的功能使用说明
6. [**配置说明**](05-configuration.md) - 配置参数详解
7. [**数据模型与序列化**](06-data-models.md) - 数据结构和协议
8. [**最佳实践**](07-best-practices.md) - 使用建议和优化技巧
9. [**故障排查**](08-troubleshooting.md) - 问题诊断和解决方案
10. [**API 参考**](09-api-reference.md) - API 文档和接口说明

### 旧版文档（已废弃）

以下文档已整合到新的文档体系中，保留仅供参考：

- `data_models.md` → 已整合到 `06-data-models.md`
- `serialization_format.md` → 已整合到 `06-data-models.md`
- `rpc_client_mvp.md` → 已整合到 `04-user-guide.md`
- `rpc_server_mvp.md` → 已整合到 `04-user-guide.md`
- `rpc_server_implementation_summary.md` → 已整合到 `03-architecture.md`

## 快速导航

### 新手入门

1. 阅读[介绍](01-introduction.md)了解框架
2. 跟随[快速开始](02-quick-start.md)创建第一个服务
3. 查看 `examples/` 目录中的示例代码

### 深入学习

1. 学习[架构设计](03-architecture.md)理解框架原理
2. 阅读[使用指南](04-user-guide.md)掌握各项功能
3. 参考[最佳实践](07-best-practices.md)优化应用

### 问题解决

1. 查看[故障排查](08-troubleshooting.md)诊断问题
2. 参考[配置说明](05-configuration.md)调整参数
3. 查阅[API 参考](09-api-reference.md)了解接口

## 生成 API 文档

### 前置要求

安装 Doxygen 和 Graphviz：

**Ubuntu/Debian:**
```bash
sudo apt-get install doxygen graphviz
```

**macOS:**
```bash
brew install doxygen graphviz
```

**Fedora/RHEL:**
```bash
sudo dnf install doxygen graphviz
```

### 生成文档

```bash
# 方法 1: 使用提供的脚本
./docs/generate_docs.sh

# 方法 2: 直接使用 doxygen
doxygen Doxyfile
```

生成的文档位于 `docs/html/` 目录，使用浏览器打开 `docs/html/index.html` 查看。

## 验证文档完整性

运行文档验证脚本检查所有公共接口是否有完整的文档注释：

```bash
python3 docs/verify_docs.py
```

该脚本会检查：
- 所有公共类是否有文档注释
- 所有公共函数是否有文档注释
- 函数文档是否包含参数说明（@param）
- 函数文档是否包含返回值说明（@return）
- 函数文档是否包含异常说明（@throws）

## 文档注释规范

FRPC 框架使用 Doxygen 风格的文档注释。所有公共接口必须包含详细的文档注释。

### 类文档注释示例

```cpp
/**
 * @brief 网络引擎 - 基于 Reactor 模式的事件驱动网络层
 * 
 * 使用 epoll 实现高效的事件多路复用，支持非阻塞 socket 操作。
 * 所有网络 I/O 操作都是异步的，通过回调或协程恢复通知完成。
 * 
 * @note 线程安全：NetworkEngine 不是线程安全的，应在单个线程中使用
 */
class NetworkEngine {
    // ...
};
```

### 函数文档注释示例

```cpp
/**
 * @brief 异步发送数据（协程友好）
 * 
 * @param fd 文件描述符
 * @param data 要发送的数据
 * @return Awaitable 对象，可用于 co_await
 * 
 * @throws NetworkException 如果 socket 无效或发送失败
 * 
 * @example
 * auto result = co_await engine.async_send(fd, data);
 * if (!result) {
 *     // 处理发送失败
 * }
 */
Awaitable<SendResult> async_send(int fd, std::span<const uint8_t> data);
```

## 贡献文档

如果您想为文档做出贡献，请遵循以下准则：

1. 使用清晰、简洁的语言
2. 提供实际的代码示例
3. 包含必要的注意事项和警告
4. 保持文档与代码同步
5. 使用 Markdown 格式编写用户文档
6. 使用 Doxygen 格式编写 API 文档注释

## 文档更新

当代码发生变化时，请确保同步更新相关文档：

1. 更新头文件中的文档注释
2. 重新生成 API 文档
3. 更新用户指南中的相关章节
4. 更新示例代码（如果受影响）
5. 运行文档验证脚本确保完整性

## 联系方式

如有文档相关问题或建议，请提交 Issue 或 Pull Request。

