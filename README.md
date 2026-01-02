# Qt 实验 4：多用户聊天室（TCP）

`server` 使用 `QTcpServer`/`QTcpSocket`（每个连接一个 `QThread`）实现多用户聊天室；`client` 为 Qt Widgets 图形客户端，协议为按行分隔的 JSON（`\n`）。

## 构建（Qt Creator）

1. 用 Qt Creator 打开 `ex4.pro`
2. 选择 Kit：`Desktop Qt 6.x MinGW 64-bit`
3. `Ctrl+B` 编译（会生成 `server`/`client` 两个可执行文件）

## 运行与演示

- 运行 `server`，点击 **Start**（默认端口 `45454`）
- 运行多个 `client`，填 `127.0.0.1:45454` + 不同昵称登录，然后发送消息即可看到广播与在线用户列表

一键启动（推荐演示）：先在 Qt Creator 至少编译一次，然后运行：

- `run-demo.bat`（默认启动 2 个客户端）
- `run-demo.bat 3`（启动 3 个客户端）

## 私聊

客户端输入以下任意一种格式：

- `/w 用户名 内容`
- `@用户名 内容`

## 说明

- 协议/限制在 `common/protocol.h`
- 若在 Windows + MinGW 下遇到 `sub-xxx-make_first Error 2`，建议把工程/构建目录放到纯英文路径，且确保使用 Qt 自带 MinGW 工具链

