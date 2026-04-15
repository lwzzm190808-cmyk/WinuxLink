# WinuxLink - 跨平台文件传输与远程控制工具 🚀

## 项目简介

WinuxLink 是一个跨平台的文件传输与远程控制工具，使用 C++ 开发，支持 Windows 和 Linux 系统。它提供了安全、高效的文件传输功能和远程命令执行能力，适用于需要远程管理和文件传输的场景。

## 功能特性

- 📁 **文件传输**：支持文件上传、下载和目录列表
- 🖥️ **远程控制**：支持执行远程命令并获取输出
- 🔒 **安全通信**：基于 TCP/IP 协议，使用自定义协议格式
- 🔄 **跨平台支持**：同时支持 Windows 和 Linux 系统
- 🧵 **多线程设计**：采用多线程架构，提高并发处理能力
- 🔧 **单例模式**：网络模块使用单例模式，简化使用
- 📊 **大文件支持**：支持分块传输大文件，带进度显示
- 📝 **完整日志**：提供详细的日志记录功能
- 🔐 **用户认证**：支持用户名和密码认证

## 系统架构

### 核心模块

- **Network**：网络通信模块，负责建立和维护网络连接
- **Agreement**：协议定义模块，定义了通信协议的格式和命令
- **PacketBuilder**：数据包构建模块，负责构建和解析数据包
- **Logger**：日志模块，提供日志记录功能
- **Transfer**：文件传输模块，负责文件的上传和下载
- **CmdManager**：命令管理模块，负责处理各种命令请求
- **Command**：命令执行模块，负责具体命令的执行逻辑

### 目录结构

```
├── bin/              # 可执行文件目录
│   ├── Winuxlink_client.exe   # Windows 客户端可执行文件
│   └── Winuxlink_server.exe   # Windows 服务器可执行文件
├── client/           # 客户端代码目录
│   ├── main.cpp      # 客户端主入口
│   └── main.o        # 客户端编译目标文件
├── core/             # 核心功能实现
│   ├── Agreement.cpp # 协议实现
│   ├── Agreement.h   # 协议定义
│   ├── CmdManager.cpp # 命令管理实现
│   ├── CmdManager.h   # 命令管理定义
│   ├── CommandVector.h # 命令向量定义
│   ├── Connector.cpp  # 连接器实现
│   ├── Connector.h    # 连接器定义
│   ├── DownloadCommand.cpp # 下载命令实现
│   ├── DownloadCommand.h   # 下载命令定义
│   ├── ExecCommand.cpp # 执行命令实现
│   ├── ExecCommand.h   # 执行命令定义
│   ├── FileInfoCommand.cpp # 文件信息命令实现
│   ├── FileInfoCommand.h   # 文件信息命令定义
│   ├── GetCommand.cpp  # 获取命令实现
│   ├── GetCommand.h    # 获取命令定义
│   ├── HeartbeatCommand.h # 心跳命令定义
│   ├── ICommand.h      # 命令接口定义
│   ├── ListCommand.cpp # 列表命令实现
│   ├── ListCommand.h   # 列表命令定义
│   ├── LoginCommand.cpp # 登录命令实现
│   ├── LoginCommand.h   # 登录命令定义
│   ├── Logger.cpp      # 日志实现
│   ├── Logger.h        # 日志定义
│   ├── Network.cpp     # 网络通信实现
│   ├── Network.h       # 网络通信定义
│   ├── NetworkPremise.h # 网络前提定义
│   ├── PacketBuilder.cpp # 数据包构建实现
│   ├── PacketBuilder.h   # 数据包构建定义
│   ├── PutCommand.cpp  # 上传命令实现
│   ├── PutCommand.h    # 上传命令定义
│   ├── QuitCommand.h   # 退出命令定义
│   ├── RequestTracker.h # 请求追踪器定义
│   ├── Transfer.cpp    # 文件传输实现
│   ├── Transfer.h      # 文件传输定义
│   ├── UploadCommand.cpp # 上传命令实现
│   ├── UploadCommand.h   # 上传命令定义
│   └── libwinuxlink_core.a # 核心库文件
├── server/           # 服务器代码目录
│   ├── main.cpp      # 服务器主入口
│   └── main.o        # 服务器编译目标文件
├── Makefile          # 项目构建文件
└── README.md         # 项目说明文档
```

## 协议设计

WinuxLink 使用自定义的通信协议，协议格式如下：

### 协议头部

```c++
struct Header {
    uint32_t magic;           // 魔数，固定为 0x4C494E4B ("LINK")
    uint16_t version;         // 协议版本号，当前为 1
    uint16_t command;         // 命令/状态码
    uint32_t payload_length;  // 载荷长度
    uint32_t sequence_id;     // 包序列号
};
```

### 命令类型

| 命令 | 值 | 描述 |
|------|-----|------|
| LIST | 0x0001 | 请求列出远程目录内容 |
| GET | 0x0002 | 请求下载文件 |
| PUT | 0x0003 | 请求上传文件 |
| EXEC | 0x0010 | 请求执行远程命令 |
| HEARTBEAT | 0xFFFF | 心跳包，用于保持连接 |
| DISCONNECT | 0xFFF0 | 断开连接命令 |
| CONNECT | 0xFFF1 | 连接命令 |
| QUIT_REQUEST | 0xFFF2 | 退出请求 |
| QUIT_RESPONSE | 0xFFF3 | 退出响应 |

### 状态码

| 状态码 | 值 | 描述 |
|--------|-----|------|
| OK_REQUEST | 0x0000 | 请求成功处理 |
| BAD_REQUEST | 0x4000 | 请求格式错误 |
| UNAUTHORIZED | 0x4001 | 未授权 |
| FORBIDDEN | 0x4003 | 禁止访问 |
| NOT_FOUND | 0x4004 | 请求的资源不存在 |
| INTERNAL_ERROR | 0x5000 | 服务端内部错误 |
| NOT_IMPLEMENTED | 0x5001 | 命令未实现 |

## 安装与构建

### 服务器构建

1. 进入项目根目录
2. 执行 `make` 命令构建服务器
3. 构建完成后，可执行文件将生成在 `bin/` 目录下

### 客户端构建

1. 进入 `client/` 目录
2. 执行 `make` 命令构建客户端
3. 构建完成后，可执行文件将生成在 `client/` 目录下

## 使用方法

### 启动服务器

```bash
# Windows
bin\Winuxlink_server.exe <ip> <port>

# Linux
./bin/Winuxlink_server <ip> <port>
```

例如：
```bash
# 在本地 8899 端口启动服务器
bin\Winuxlink_server.exe 127.0.0.1 8899
```

### 客户端连接

```bash
# Windows
client\Winuxlink_client.exe <ip> <port> [options]

# Linux
./client/Winuxlink_client <ip> <port> [options]
```

#### 客户端选项

- `-h, --help`：显示帮助信息
- `-u, --user <name>`：登录用户名（可选）
- `-p, --pass <pass>`：登录密码（可选）

例如：
```bash
# 连接到本地服务器并自动登录
client\Winuxlink_client.exe 127.0.0.1 8899 -u admin -p password
```

### 交互式命令

连接成功后，客户端进入交互式模式，支持以下命令：

| 命令 | 描述 | 示例 |
|------|------|------|
| `login <user> <pass>` | 登录到服务器 | `login admin password` |
| `list [path]` | 列出远程目录内容 | `list /home` |
| `get <remote> [local]` | 下载文件 | `get /home/file.txt local.txt` |
| `put <local> [remote]` | 上传文件 | `put local.txt /home/file.txt` |
| `exec <command>` | 执行远程命令 | `exec ls -la` |
| `quit / exit` | 断开连接并退出 | `quit` |
| `help / ?` | 显示帮助信息 | `help` |
| `clear / cls` | 清除屏幕 | `clear` |

### 大文件传输

对于大于 10MB 的文件，系统会自动使用分块传输模式，支持：
- 断点续传
- 进度显示
- 校验和验证

## 技术栈

- **语言**：C++
- **网络库**：Winsock2 (Windows) / POSIX sockets (Linux)
- **线程库**：C++标准线程库
- **构建工具**：Make
- **编译器**：GCC / MSVC

## 开发状态

### 已实现功能

- ✅ 网络通信模块
- ✅ 协议定义与解析
- ✅ 命令系统
- ✅ 文件传输（支持大文件分块传输）
- ✅ 远程命令执行
- ✅ 用户认证
- ✅ 日志系统
- ✅ 跨平台支持

## 许可证

本项目采用 GNU Affero General Public License v3.0 许可证，详见 LICENSE 文件。

⭐ 如果您觉得这个项目有用，请给它点个星！
