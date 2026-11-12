# IndustrialHub

基于 Qt 6 Widgets + CMake 的工业设备管理上位机项目。

## 当前版本

- Qt：6.5 或更高版本
- C++：17
- 构建：CMake
- UI：Qt Designer（`.ui`）
- 数据库：SQLite（Qt SQL 模块）
- 网络：Qt Network（TCP）
- 当前版本：0.5.0

## 已实现

### 设备管理

- 设备信息统一由 `DeviceManager` 管理，页面不直接维护设备数据副本。
- 新增设备：设备编号、设备名称必填，设备编号不区分大小写且不可重复。
- 编辑设备：设备编号作为主键，编辑时只读；添加时间保留。
- 删除设备：二次确认。
- 查询设备：支持按设备编号、名称、类型、地址模糊搜索。
- 状态筛选：全部、在线、离线、故障。
- 状态更新：管理层提供按设备编号更新状态的接口。
- Qt signals/slots：设备增删改和状态变化后发出信号，页面自动刷新。

### SQLite 数据持久化

- 使用 Qt SQL 的 QSQLITE 驱动。
- 首次启动自动创建数据库目录和 `devices` 数据表。
- 程序启动时从数据库加载设备。
- 新增、编辑、删除和状态修改均先写入数据库，成功后再更新内存列表。
- 设备编号使用 SQLite `PRIMARY KEY COLLATE NOCASE`，防止大小写不同的重复编号。
- 仅当数据库为空时写入两条演示设备，不会在每次启动时覆盖用户数据。
- 数据库文件名为 `industrialhub.db`，存放在 Qt `QStandardPaths::AppDataLocation` 对应的应用数据目录。

### 统一通信接口

- `ICommunication` 抽象基类统一定义连接、断开、连接状态、发送数据和接收数据接口。
- `SimulatedCommunication` 提供回环模拟实现，无需真实设备即可验证上层通信调用。
- `TcpCommunication` 提供 TCP 客户端实现，支持主机名/IPv4 地址和 IPv6 地址。
- TCP 设备地址格式：`主机名:端口`、`IPv4:端口` 或 `[IPv6]:端口`。
- TCP 连接采用异步方式建立；`open()` 返回 true 表示连接请求已发起，连接成功后通过 `connectionStateChanged(true)` 通知。
- TCP 接收数据通过 `dataReceived` 信号上报；发送前会检查连接状态和写入结果。

### 自动化通信测试

- `communication_smoke_test`：验证模拟通信的打开、发送、回环接收和关闭。
- `tcp_communication_smoke_test`：通过本机 `QTcpServer` 验证 TCP 连接、数据收发、断开和非法端口校验。
- 两个测试均不需要真实设备或外部网络服务。

## Windows 构建

建议使用 VS Code + CMake Tools，选择 Qt 6 MinGW 64-bit 工具链。

也可以在已配置 Qt 6 CMake 环境的终端中执行：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.10.3/mingw_64"
cmake --build build --config Debug
```

请按本机 Qt 安装目录调整 `CMAKE_PREFIX_PATH`。

## 目录说明

- `app/`：程序入口
- `src/pages/`：功能页面、设备数据结构和设备管理逻辑
- `src/database/`：数据库持久化实现
- `src/communication/`：统一通信接口、模拟通信和 TCP 实现
- `src/ui/MainWindow/`：主窗口
- `CustomFolder/`：自定义控件
- `tests/`：通信层独立冒烟测试

## 后续开发建议

1. 将通信管理器接入设备管理页面，提供连接/断开/发送/接收交互。
2. 增加 UDP 和 RS485 串口通信实现。
3. 增加设备在线心跳、超时离线和故障状态联动。
4. 增加日志、报警和历史数据存储。
