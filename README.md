# IndustrialHub

基于 Qt 6 Widgets + Qt Designer + CMake 的工业设备管理上位机项目。

## 当前版本

- Qt：6.5 或更高版本
- C++：17
- 构建：CMake + Ninja
- UI：Qt Widgets / Qt Designer（`.ui`）
- 数据库：SQLite（Qt SQL）
- 网络：Qt Network
- 当前版本：0.9.0

## 已实现模块

### 设备管理与数据持久化

- `DeviceManager` 统一管理设备信息，支持新增、编辑、删除、查询和状态更新。
- 使用 QSQLITE，首次启动自动创建数据库目录和数据表。
- 程序启动时从数据库加载设备；增删改操作先写数据库，成功后更新内存。
- 设备编号忽略大小写，避免重复编号。
- 程序重启时将上次运行遗留的在线状态重置为离线。

### 通信接口与驱动

- `ICommunication` 抽象基类统一定义打开、关闭、状态、发送和接收接口。
- `SimulatedCommunication`：无硬件回环模拟。
- `TcpCommunication`：TCP 客户端，支持主机名、IPv4 和 `[IPv6]:端口`。
- `UdpCommunication`：UDP 数据报，支持 IPv4 和 `[IPv6]:端口` 数字地址。
- `Rs485Communication`：基于 Qt SerialPort 的串口收发，支持波特率、数据位、校验位、停止位和流控。
- `SlcanCommunication`：通过 Qt SerialPort 驱动 CANable 2.0 SLCAN，支持经典 CAN 与 CAN-FD。
- SLCAN 地址格式：经典 CAN 使用 `COM4,1000000`；CAN-FD 使用 `COM4,1000000,2000000`。串口波特率按当前适配器配置为 1 Mbps。
- CAN 仲裁段支持 10k、20k、50k、100k、125k、250k、500k、800k、1M；CAN-FD 数据段支持 2M 或 5M。
- CAN 经典帧支持标准/扩展 ID；CAN-FD 支持标准/扩展 ID、BRS 和 0-64 字节合法 DLC 长度映射。
- SLCAN 帧命令使用 `t/T`（经典 CAN）、`d/D`（CAN-FD 无 BRS）、`b/B`（CAN-FD 启用 BRS）。标准 ID 使用小写，扩展 ID 使用大写；发送时界面输入不需要手动添加回车符。
- 示例：经典扩展帧 `T18AABBCC03112233`；CAN-FD 标准帧（12 字节）`d7E09112233445566778899AABBCC`。
- Qt SerialPort 是可选模块：未安装时主程序仍可构建，但 RS485 和 SLCAN 驱动不会注册。

### 多设备通信管理与 UI

- `CommunicationManager` 按设备编号管理独立通信实例，支持多个设备同时工作、单设备断开、全部断开、数据发送和信号转发。
- 通信类型使用工厂注册；TCP、UDP、模拟通信默认可用，安装 Qt SerialPort 后自动注册 RS485、CAN 和 CANFD。
- 通信配置页面支持选择设备、连接/断开、发送 HEX 或 SLCAN 帧命令，并查看 TX/RX 和错误日志。
- TCP/模拟连接状态会更新设备状态；UDP、RS485、CAN/CAN-FD 收到实际数据后才标记设备在线。

## 自动化测试

启用 CMake 的 `BUILD_TESTING` 时构建：

- `communication_smoke_test`：模拟通信回环。
- `tcp_communication_smoke_test`：TCP 本机回环。
- `udp_communication_smoke_test`：UDP 本机数据报回环。
- `communication_manager_smoke_test`：多设备管理、数据路由和资源释放。
- `rs485_communication_smoke_test`：Qt SerialPort 可用时，检查串口参数格式。
- `slcan_frame_codec_test`：SLCAN 经典 CAN/CAN-FD 帧编解码、DLC 映射和非法帧校验。
- `slcan_communication_smoke_test`：Qt SerialPort 可用时，检查适配器参数校验。

构建后运行：

```powershell
ctest --test-dir build --output-on-failure
```

TCP/UDP/模拟测试使用本机回环，不需要真实设备；串口配置测试也不要求连接硬件。

## Windows 构建

建议使用 VS Code + CMake Tools，选择 Qt 6 MinGW 64-bit 工具链。

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:/qt/6.10.3/mingw_64"
cmake --build build --config Debug
ctest --test-dir build --output-on-failure
```

请按本机 Qt 安装目录调整 `CMAKE_PREFIX_PATH`。如果没有安装 Qt SerialPort，CMake 会提示串口驱动被禁用，不影响 TCP/UDP 和主程序。

## 目录结构

- `app/`：程序入口
- `src/pages/`：功能页面、设备数据结构和设备管理逻辑
- `src/database/`：SQLite 数据持久化
- `src/communication/`：通信接口、TCP/UDP/RS485/SLCAN 驱动和通信管理器
- `src/ui/MainWindow/`：主窗口
- `CustomFolder/`：自定义控件
- `tests/`：通信层自动化测试

## 后续路线

1. 设备协议解析层、命令模板和参数配置。
2. 心跳检测、超时离线和故障状态联动。
3. 统一日志、报警和历史数据存储。
4. 实时监控页面接入设备数据流、曲线和数据记录。
5. 完善重连策略、设备配置持久化、运行日志和程序部署。
