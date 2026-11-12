# IndustrialHub

基于 Qt 6 Widgets + CMake 的工业设备管理上位机项目。

## 当前版本

- Qt：6.5 或更高版本
- C++：17
- 构建：CMake
- UI：Qt Designer（`.ui`）
- 数据库：SQLite（Qt SQL 模块）
- 网络：Qt Network（TCP）
- 当前版本：0.6.0

## 已实现

### 设备管理与数据持久化

- `DeviceManager` 统一管理设备信息，支持新增、编辑、删除、查询和状态更新。
- 使用 Qt SQL 的 QSQLITE 驱动；首次启动自动创建数据库目录和数据表。
- 设备数据启动时从数据库加载，修改先写数据库，成功后更新内存。
- 设备编号使用 SQLite 主键并忽略大小写，避免重复编号。
- 数据库位于 Qt `QStandardPaths::AppDataLocation` 对应的应用数据目录。

### 通信接口与驱动

- `ICommunication` 抽象基类统一定义连接、断开、连接状态、发送和接收接口。
- `SimulatedCommunication` 提供无硬件回环模拟。
- `TcpCommunication` 提供 TCP 客户端实现，支持主机名、IPv4 和 `[IPv6]:端口` 格式。
- TCP 连接异步建立；连接成功、断开、接收数据和错误均通过信号上报。
- `CommunicationManager` 按设备编号管理独立通信实例，支持多个设备同时连接、单设备断开、全部断开、数据发送和信号转发。
- 通信类型通过工厂注册，后续可扩展 UDP、RS485、CAN/CANFD，而不让业务页面依赖具体驱动类。
- 设备编号查找忽略大小写，与设备数据库的编号规则保持一致。

### 自动化测试

启用 CMake 的 `BUILD_TESTING` 时会构建以下测试：

- `communication_smoke_test`：模拟通信打开、发送、回环接收和关闭。
- `tcp_communication_smoke_test`：本机 TCP 连接、数据收发、断开和非法端口检查。
- `communication_manager_smoke_test`：验证管理器同时管理模拟设备和 TCP 设备、按设备编号路由数据，以及单设备断开不影响其他设备。

测试使用本机回环服务，不需要真实设备或外部服务器。可在构建后运行：

```powershell
ctest --test-dir build --output-on-failure
```

## Windows 构建

建议使用 VS Code + CMake Tools，选择 Qt 6 MinGW 64-bit 工具链。

也可以在已配置 Qt 6 CMake 环境的终端中执行：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:/qt/6.10.3/mingw_64"
cmake --build build --config Debug
```

请按本机 Qt 安装目录调整 `CMAKE_PREFIX_PATH`。

## 目录说明

- `app/`：程序入口
- `src/pages/`：功能页面、设备数据结构和设备管理逻辑
- `src/database/`：SQLite 数据持久化实现
- `src/communication/`：通信接口、驱动和多设备通信管理器
- `src/ui/MainWindow/`：主窗口
- `CustomFolder/`：自定义控件
- `tests/`：通信层自动化冒烟测试

## 后续开发路线

1. 将通信管理器接入设备管理页面，增加连接、断开、发送和接收日志界面。
2. 增加 UDP 和 RS485 串口通信实现。
3. 增加设备在线心跳、超时离线和故障状态联动。
4. 增加日志、报警和历史数据存储。
5. 增加 CAN/CANFD 通信驱动及设备协议解析层。
