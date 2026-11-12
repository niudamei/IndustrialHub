# IndustrialHub

基于 Qt 6 Widgets + CMake 的工业设备管理上位机项目。

## 当前版本

- Qt：6.5 或更高版本
- C++：17
- 构建：CMake
- UI：Qt Designer（`.ui`）
- 数据库：SQLite（Qt SQL 模块）
- 当前版本：0.4.0

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

### 统一通信接口（基础层）

- 新增 `ICommunication` 抽象基类，统一定义连接、断开、连接状态、发送数据和接收数据接口。
- 新增 `SimulatedCommunication` 回环模拟实现，无需真实设备即可验证上层通信调用。
- 模拟器发送数据后会延迟约 30 ms，通过 `dataReceived` 信号回传相同数据。
- 后续 TCP、UDP、RS485、CAN/CANFD 驱动可分别实现 `ICommunication`，不需要让页面依赖具体驱动类。

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
- `src/communication/`：统一通信接口及模拟通信实现
- `src/ui/MainWindow/`：主窗口
- `CustomFolder/`：自定义控件

## 后续开发建议

1. 增加数据库备份、恢复和版本迁移。
2. 基于统一通信接口实现 TCP/UDP 网络通信及串口 RS485 驱动。
3. 增加设备在线心跳、超时离线和故障状态联动。
4. 增加日志、报警和历史数据存储。
