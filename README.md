# IndustrialHub

基于 Qt 6 Widgets + Qt Designer + CMake 的工业设备管理上位机项目。

## 当前版本

- Qt：6.5 或更高版本
- C++：17
- 构建：CMake + Ninja
- UI：Qt Widgets / Qt Designer（`.ui`）
- 数据库：SQLite（Qt SQL）
- 网络：Qt Network
- 当前版本：0.8.0

## 已实现模块

### 设备管理与数据持久化

- `DeviceManager` 统一管理设备信息，支持新增、编辑、删除、查询和状态更新。
- 使用 QSQLITE，首次启动自动创建数据库目录和数据表。
- 程序启动时从数据库加载设备；增删改操作先写数据库，成功后更新内存。
- 设备编号忽略大小写，避免重复编号。
- 数据库位于 Qt `QStandardPaths::AppDataLocation` 对应的应用数据目录。
- 程序重启时将上次运行遗留的在线状态重置为离线，避免显示过期连接状态。

### 通信接口与驱动

- `ICommunication` 抽象基类统一定义打开、关闭、状态、发送和接收接口。
- `SimulatedCommunication`：无硬件回环模拟，便于验证上层业务。
- `TcpCommunication`：TCP 客户端，支持主机名、IPv4 和 `[IPv6]:端口`。
- `UdpCommunication`：UDP 数据报，支持 IPv4 和 `[IPv6]:端口` 数字地址。
- `Rs485Communication`：基于 Qt SerialPort 的串口收发，支持波特率、数据位、校验位、停止位和可选流控。
- RS485 设备地址格式：`COM3,115200,8,N,1`，最后一项流控可选：N 无、H 硬件、S 软件。
- UDP 打开表示本地 socket 已绑定，RS485 打开表示本地串口已打开；二者都不代表远端设备已在线。收到设备实际数据后才会更新在线状态。
- Qt SerialPort 是可选模块：本机未安装时，主程序仍可构建，但 RS485 驱动不会注册。

### 多设备通信管理

- `CommunicationManager` 按设备编号管理独立通信实例，支持多个设备同时工作。
- 支持单设备连接/断开、全部断开、数据发送、数据接收和错误信号转发。
- 通信类型使用工厂注册；TCP、UDP、模拟通信默认可用，安装 Qt SerialPort 后自动注册 RS485/SERIAL。
- 设备编号查找忽略大小写。
- 通信配置页面已经接入管理器：选择设备、连接/断开、发送十六进制数据、查看 TX/RX 和错误日志。
- TCP/模拟通信连接状态、以及 UDP/RS485 收到的实际数据会同步设备在线状态；删除设备时释放其通信实例。

## 自动化测试

启用 CMake 的 `BUILD_TESTING` 时构建：

- `communication_smoke_test`：模拟通信打开、发送、回环接收和关闭。
- `tcp_communication_smoke_test`：本机 TCP 连接、收发、断开和非法端口检查。
- `udp_communication_smoke_test`：本机 UDP 数据报回环和非法端口检查。
- `communication_manager_smoke_test`：验证多设备管理、数据路由、单设备断开和资源释放。
- `rs485_communication_smoke_test`：仅在 Qt SerialPort 可用时构建，检查串口参数格式，不要求连接真实串口。

构建后运行：

```powershell
ctest --test-dir build --output-on-failure
```

TCP/UDP/模拟测试使用本机回环，不需要真实设备或外部服务器。

## Windows 构建

建议使用 VS Code + CMake Tools，选择 Qt 6 MinGW 64-bit 工具链。

也可以在已配置 Qt 6 CMake 环境的终端执行：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:/qt/6.10.3/mingw_64"
cmake --build build --config Debug
ctest --test-dir build --output-on-failure
```

请按本机 Qt 安装目录调整 `CMAKE_PREFIX_PATH`。如果没有安装 Qt SerialPort，CMake 会提示 RS485 驱动被禁用，不影响 TCP/UDP 和主程序。

## 目录结构

- `app/`：程序入口
- `src/pages/`：功能页面、设备数据结构和设备管理逻辑
- `src/database/`：SQLite 数据持久化
- `src/communication/`：通信接口、驱动和多设备通信管理器
- `src/ui/MainWindow/`：主窗口
- `CustomFolder/`：自定义控件
- `tests/`：通信层自动化测试

## 后续路线

1. CAN/CANFD 驱动及帧格式适配。
2. 设备协议解析层、命令模板和参数配置。
3. 心跳检测、超时离线和故障状态联动。
4. 统一日志、报警和历史数据存储。
5. 实时监控页面接入设备数据流、曲线和数据记录。
6. 完善设备连接状态恢复、重连策略和程序部署。
