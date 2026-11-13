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

### 通信历史数据与统一数据入口

- `DataManager` 统一接收 `CommunicationManager` 发出的 TX/RX 事件。
- 通信记录采用批量事务写入 SQLite，保存毫秒时间戳、设备编号、方向、原始字节和字节数。
- 历史数据页面支持日期范围、设备编号、TX/RX 方向筛选，按 200 条分页查询。
- 支持导出当前页 CSV（UTF-8 BOM），便于使用 Excel 查看。
- 当前保存的是通信原始帧，不会在缺少设备协议定义时臆测温度、电流、位置等业务字段。

### 统一业务数据模型与协议解析基础

- `BusinessDataPoint` 统一描述设备编号、变量编号、变量名称、数值、单位、时间戳、质量状态和来源协议。
- `IProtocolParser` 定义协议解析接口；具体设备协议通过独立解析器实现，不耦合通信驱动。
- `ProtocolManager` 支持注册解析器、按设备编号绑定解析器，并通过 Qt signal 发布解析结果。
- 当前提供 `JsonProtocolParser` 作为通用 JSON 示例解析器，支持 ISO 8601 时间戳、多个业务变量和 Good/Uncertain/Bad 质量状态。
- JSON 示例解析器不会自动套用到 CAN、CAN-FD 或私有 RS485 报文。真实设备必须在拿到协议文档后单独实现和绑定解析器。
- 解析采用整帧校验：任意变量字段不合法时整帧失败，不输出半帧数据。

JSON 示例：

```json
{
  "timestamp": "2026-11-13T10:20:30.123Z",
  "values": [
    {"id": "temperature", "name": "温度", "value": 26.5, "unit": "°C", "quality": "good"},
    {"id": "rpm", "name": "转速", "value": 1200, "unit": "rpm", "quality": "uncertain"}
  ]
}
```

### 业务数据管理与通信链路接入

- 主窗口创建 `ProtocolManager` 和 `BusinessDataManager`，将通信 RX 信号交给协议管理器。
- 通信配置页可以为当前设备选择“不解析原始数据”或“JSON 示例协议”；默认是不解析。
- 当前协议绑定仅在本次程序运行期间有效；重新启动后默认恢复为不解析，避免未经确认的协议配置长期生效。
- 只有显式绑定 JSON 的设备，其 RX payload 才会进入 JSON 解析器。未绑定的 CAN/CAN-FD/RS485 等设备仍只保留原始通信帧。
- 解析成功后，业务数据管理器按设备编号和变量编号保存最新值；较旧时间戳的数据不会覆盖较新的值。
- 通信日志会显示解析后的变量名称、变量编号、数值、单位和质量状态。
- 当前业务数据最新值缓存仅保存在内存中，程序重启后会清空；业务变量历史入库仍待实现。

## 自动化测试

启用 CMake 的 `BUILD_TESTING` 时构建：

- `communication_smoke_test`：模拟通信回环。
- `tcp_communication_smoke_test`：TCP 本机回环。
- `udp_communication_smoke_test`：UDP 本机数据报回环。
- `communication_manager_smoke_test`：多设备管理、数据路由和资源释放。
- `rs485_communication_smoke_test`：Qt SerialPort 可用时，检查串口参数格式。
- `slcan_frame_codec_test`：SLCAN 经典 CAN/CAN-FD 帧编解码、DLC 映射和非法帧校验。
- `slcan_communication_smoke_test`：Qt SerialPort 可用时，检查适配器参数校验。
- `datamanager_history_test`：检查 TX/RX 事件入库、历史筛选和原始字节回读。
- `json_protocol_parser_test`：检查 JSON 业务变量解析、质量状态、设备绑定、信号分发和非法报文拒绝。
- `business_data_manager_test`：检查业务变量最新值、大小写不敏感查询、旧时间戳保护和清理。

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
- `src/database/`：SQLite 设备信息持久化
- `src/core/`：统一数据入口、通信历史记录和业务数据管理
- `src/protocol/`：业务数据模型、协议解析接口和具体协议解析器
- `src/communication/`：通信接口、TCP/UDP/RS485/SLCAN 驱动和通信管理器
- `src/ui/MainWindow/`：主窗口
- `CustomFolder/`：自定义控件
- `tests/`：通信层与数据持久化自动化测试
- `docs/PROJECT_ROADMAP.md`：阶段完成情况与后续开发路线

## 后续路线

1. 建立设备协议解析层和统一业务数据模型，将原始帧解析为带设备、变量、数值和时间戳的数据点。
2. 基于统一业务数据实现阈值报警、确认、恢复和报警历史。
3. 将实时曲线与历史数据查询从原始帧逐步升级为业务变量趋势。
4. 完善通信线程隔离、超时离线、重连策略和设备配置持久化。
5. 增加 Modbus、MQTT、WebSocket；按真实设备协议逐步验证，不虚构寄存器或报文格式。
6. 建设用户角色、操作审计和插件接口。
7. 完善 GitHub Actions、自动化测试、安装包和部署文档。


## 报警管理（当前阶段）

业务解析输出的 `BusinessDataPoint` 会并行进入 `AlarmManager`。报警规则支持设备/变量匹配、低限/高限、启用状态和 Info/Warning/Critical 等级；仅 `Good` 质量且为有限数值的数据参与阈值判断。报警事件支持 ACTIVE、ACKNOWLEDGED、RECOVERED、RECOVERED_ACK 状态，并持久化到与通信历史共用的 SQLite 文件。报警中心可查看事件并填写确认人。

报警中心提供新增阈值规则入口，规则与报警事件均持久化到 SQLite；当前已有新增和确认操作，规则编辑/删除/启停及报警筛选仍待完善。JSON 示例协议仍需用户显式绑定到设备，不能将未知 CAN/RS485 报文当作业务变量解析。


## 模拟业务变量与报警演示

在“实时监控 → 模拟设备（独立演示区）”启动模拟后，系统会为三台虚拟设备分别生成温度、转速和电流业务变量，并送入统一的 `BusinessDataManager` 与 `AlarmManager`。勾选“温度超限演示（触发报警）”后，模拟温度将升至 95 °C，超过首次运行自动创建的 80 °C 高限规则，可在“报警中心”观察报警触发；取消勾选后，温度恢复并自动恢复报警状态。

这组业务变量是明确标记为 `SIMULATED-BUSINESS` 的演示数据，与模拟 CAN/TCP 原始通信帧分离，不代表任何真实设备协议或硬件测量值。模拟设备断线时，该设备不再生成业务变量。


## 业务变量实时趋势

“实时趋势”页面除原有通信帧率外，新增温度、转速、电流三个独立曲线页签。曲线从统一的 `BusinessDataManager` 读取最新业务数据，按设备筛选并展示最近 60 秒趋势，同时计算当前值、最大值、最小值和平均值。仅质量状态为 Good 且时间戳有效的数据参与曲线统计；无有效数据时会显示提示。模拟数据需先在“实时监控 → 模拟设备”中启动。


## 业务变量历史存储

业务变量在进入 `BusinessDataManager` 最新值缓存的同时，也会异步批量写入 SQLite 的独立 `business_data_history` 表。历史记录保留设备 ID、变量 ID/名称、数值、单位、时间戳、质量状态和来源协议，并建立时间及设备/变量/时间索引。通信原始帧仍保存在 `communication_history`，两类数据互不混用。

`DataManager::queryBusinessDataHistory` 提供按时间范围、设备 ID、变量 ID 查询和分页接口；“历史数据”页面已接入业务变量历史筛选、曲线回放与当前页 CSV 导出。


## 业务变量历史页面

“历史数据”页面现分为“通信原始帧”和“业务变量历史”两个页签。业务变量历史支持开始/结束时间、设备编号、变量类型筛选，分页显示时间、设备、变量、数值、单位、质量状态和来源协议；当前页数据可绘制历史曲线并导出带 UTF-8 BOM 的 CSV。CSV 导出范围为当前页（每页最多 200 条），可通过翻页分批导出。

### 报警中心规则管理

报警中心提供阈值规则列表，可新增、编辑、删除及启用/禁用规则；报警事件支持按设备 ID、等级和状态筛选，展示活动报警数、今日报警数及平均持续时间，并可将当前筛选结果导出为 UTF-8 CSV。禁用规则会阻止新报警产生，但已有活动事件仍会在后续业务数据恢复到阈值范围时正常关闭。
