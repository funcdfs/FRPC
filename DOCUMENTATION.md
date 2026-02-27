# FRPC 框架文档指南

## 📚 文档结构

项目文档已完全重组，采用清晰的编号体系和逻辑结构。

### 核心文档（docs/ 目录）

```
docs/
├── 00-index.md              # 📖 文档索引 - 从这里开始
├── 01-introduction.md       # 🎯 框架介绍
├── 02-quick-start.md        # 🚀 快速开始（5分钟上手）
├── 03-architecture.md       # 🏗️  架构设计
├── 04-user-guide.md         # 📘 使用指南
├── 05-configuration.md      # ⚙️  配置说明
├── 06-data-models.md        # 📦 数据模型与序列化
├── 07-best-practices.md     # ⭐ 最佳实践
├── 08-troubleshooting.md    # 🔧 故障排查
└── 09-api-reference.md      # 📚 API 参考
```

### 辅助文件

```
docs/
├── README.md                # 文档目录说明
├── generate_docs.sh         # API 文档生成脚本
├── verify_docs.py           # 文档完整性验证
└── archive/                 # 归档的旧文档
```

## 🎯 快速导航

### 新手入门

1. **了解框架** → [01-introduction.md](docs/01-introduction.md)
   - 什么是 FRPC
   - 核心特性
   - 适用场景

2. **快速上手** → [02-quick-start.md](docs/02-quick-start.md)
   - 安装和构建
   - 第一个 RPC 服务
   - 运行示例

3. **查看示例** → [examples/](examples/)
   - Hello World
   - 完整 RPC 示例
   - 各种服务示例

### 深入学习

4. **理解架构** → [03-architecture.md](docs/03-architecture.md)
   - 整体架构
   - 核心组件
   - 调用流程

5. **掌握功能** → [04-user-guide.md](docs/04-user-guide.md)
   - RPC 服务端/客户端
   - 服务治理
   - 协程编程

6. **优化应用** → [07-best-practices.md](docs/07-best-practices.md)
   - 服务设计
   - 性能优化
   - 安全性

### 日常参考

7. **配置参数** → [05-configuration.md](docs/05-configuration.md)
8. **数据格式** → [06-data-models.md](docs/06-data-models.md)
9. **API 查询** → [09-api-reference.md](docs/09-api-reference.md)
10. **问题解决** → [08-troubleshooting.md](docs/08-troubleshooting.md)

## 📖 阅读路径

### 路径 1: 快速上手（15分钟）

```
README.md → 01-introduction.md → 02-quick-start.md → examples/
```

### 路径 2: 全面学习（2小时）

```
00-index.md → 01-introduction.md → 02-quick-start.md 
→ 03-architecture.md → 04-user-guide.md → 07-best-practices.md
```

### 路径 3: 问题解决（按需）

```
08-troubleshooting.md → 05-configuration.md → 04-user-guide.md
```

## 🔍 查找信息

### 按主题查找

| 主题 | 文档 |
|------|------|
| 安装和构建 | [02-quick-start.md](docs/02-quick-start.md) |
| 创建服务 | [02-quick-start.md](docs/02-quick-start.md#第一个-rpc-服务) |
| 服务发现 | [04-user-guide.md](docs/04-user-guide.md#服务注册与发现) |
| 负载均衡 | [04-user-guide.md](docs/04-user-guide.md#负载均衡) |
| 健康检测 | [04-user-guide.md](docs/04-user-guide.md#健康检测) |
| 连接池 | [04-user-guide.md](docs/04-user-guide.md#连接池) |
| 协程编程 | [04-user-guide.md](docs/04-user-guide.md#协程编程) |
| 错误处理 | [04-user-guide.md](docs/04-user-guide.md#错误处理) |
| 性能优化 | [04-user-guide.md](docs/04-user-guide.md#性能优化) |
| 配置参数 | [05-configuration.md](docs/05-configuration.md) |
| 数据格式 | [06-data-models.md](docs/06-data-models.md) |
| 序列化 | [06-data-models.md](docs/06-data-models.md#序列化格式) |

### 按问题查找

| 问题 | 解决方案 |
|------|----------|
| 连接失败 | [08-troubleshooting.md](docs/08-troubleshooting.md#问题-客户端无法连接到服务器) |
| 请求超时 | [08-troubleshooting.md](docs/08-troubleshooting.md#问题-请求频繁超时) |
| 性能低 | [08-troubleshooting.md](docs/08-troubleshooting.md#问题-qps-低于预期) |
| 内存泄漏 | [08-troubleshooting.md](docs/08-troubleshooting.md#问题-内存泄漏) |
| 编译错误 | [08-troubleshooting.md](docs/08-troubleshooting.md#编译问题) |

## 📝 文档特点

### ✅ 统一编号

所有核心文档使用 `00-09` 编号，便于：
- 按顺序阅读
- 快速定位
- 文件排序

### ✅ 清晰层次

文档分为三个层次：
- **入门层**: 介绍、快速开始
- **核心层**: 架构、使用指南、配置
- **参考层**: 数据模型、最佳实践、故障排查、API

### ✅ 完善引用

所有文档之间都有清晰的交叉引用链接，便于深入学习。

### ✅ 统一格式

所有文档遵循统一的 Markdown 格式规范。

## 🛠️ 生成 API 文档

```bash
# 生成 Doxygen 文档
./docs/generate_docs.sh

# 查看文档
open docs/html/index.html
```

## ✅ 验证文档

```bash
# 验证文档完整性
python3 docs/verify_docs.py
```

## 📦 归档文档

旧版本的文档已移至归档目录：

- `archive/` - 根目录的历史文档
- `docs/archive/` - docs 目录的旧文档

这些文档仅供参考，请使用新的文档体系。

## 🤝 贡献文档

如需更新或添加文档，请：

1. 遵循现有格式规范
2. 更新相关的交叉引用
3. 运行文档验证脚本
4. 提交 Pull Request

## 📞 获取帮助

- 查看 [08-troubleshooting.md](docs/08-troubleshooting.md)
- 搜索 GitHub Issues
- 提交新 Issue
- 查看 API 文档

---

**开始阅读**: [docs/00-index.md](docs/00-index.md)
