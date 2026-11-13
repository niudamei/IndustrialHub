# IndustrialHub 项目路线与完成情况

> 基准规划：项目总流程文档《IndustrialHub——工业设备监控与智能运维平台》
> 当前审查日期：2026-11-13
> 当前开发分支：`fix/simulated-communication-validation`

本文用于跟踪实际代码状态。**“页面已存在”不等于对应业务模块已经完成**；只有形成可运行的数据链路并有测试验证，才视为该功能完成。

## 阶段状态

| 阶段 | 状态 | 已有实现 | 主要缺口 |
|---|---|---|---|
| V1.0 主窗口、导航、主题、CMake | 基本完成 | Qt Widgets、Qt Designer UI、主窗口导航、CMake/Ninja 构建入口 | 统一主题和设置项仍需系统化整理 |
| V1.1 设备管理、模拟设备 | 基本完成 | 设备 CRUD、SQLite 设备持久化、设备状态管理、模拟通信、独立模拟设备区 | 设备分组、批量操作、设备配置模板 |
| V1.2 多线程通信、串口、TCP、协议解析 | 部分完成 | TCP、UDP、RS485、CAN/CAN-FD 驱动；多设备通信管理；SLCAN 编解码；协议解析器接口、设备绑定分发和 JSON 示例解析器 | 真实设备协议解析器、通信工作线程隔离、心跳与重连策略 |
| V1.3 实时曲线、数据缓存、报警 | 部分完成 | 实时通信监控、通信帧率曲线、通信统计、模拟丢帧/断线注入；统一 BusinessDataPoint 数据结构；独立模拟业务变量（温度/转速/电流）接入缓存与报警；模拟温度超限可触发报警 | 业务变量曲线、业务变量历史 |
| V1.4 SQLite、历史数据、CSV | 部分完成 | SQLite 设备表；TX/RX 原始帧批量入库；时间/设备/方向筛选、分页、当前页 CSV | 业务变量历史、跨页/全量导出、数据保留策略、查询性能验证 |
| V1.5 CAN、Modbus、MQTT、WebSocket | 部分完成 | CAN/CAN-FD、RS485、TCP、UDP | Modbus、MQTT、WebSocket；需要真实协议/服务端参数后集成验证 |
| V1.6 用户权限、日志审计、插件 | 未开始 | 尚无完整业务实现 | 登录、角色权限、操作审计、QPluginLoader 插件接口 |
| V2.0 自动测试、安装包、远程服务 | 部分完成 | 通信层 smoke tests、SLCAN codec test、DataManager 历史入库测试目标、CMake install/deploy 配置 | 实际构建验证、GitHub Actions、覆盖率、安装包验收、远程服务 |

## 当前数据链路

目前已实现：

`CommunicationManager` → `MonitorPage / TrendPage`

以及本次补充的：

`CommunicationManager TX/RX` → `DataManager` → `SQLite communication_history` → `HistoryPage`

另已建立并接入业务解析链路：

`CommunicationManager RX` → `ProtocolManager` → `IProtocolParser` → `BusinessDataPoint` → `BusinessDataManager（最新值内存缓存）`

模拟业务数据另有独立链路：`MonitorPage 模拟业务变量` → `BusinessDataPoint` → `BusinessDataManager / AlarmManager`；它不伪装成 CAN/TCP 原始帧，也不经过真实协议解析器。

通信配置页可按设备显式绑定 JSON 示例解析器；默认不解析。当前业务变量最新值只保存在内存，尚未接入业务变量历史存储和实时趋势页面。历史页仍展示通信原始帧，不对未知设备协议做猜测性解码。

## 后续开发顺序

### P0：验证当前历史记录闭环
- 在 Qt 6.10.3 MinGW 64-bit 环境完成 CMake 配置、编译和 CTest。
- 验证模拟设备 TX/RX 能进入历史表，程序重启后记录仍存在。
- 验证时间、设备、方向筛选、分页和 CSV 中文显示。
- 检查高频数据下批量写入、队列积压和 SQLite 错误处理。

### P1：建立统一业务数据模型（基础框架已提交，待接入）
- [x] 定义标准数据点：设备 ID、变量 ID/名称、数值、单位、时间戳、质量状态和来源协议。
- [x] 定义协议解析器接口与按设备绑定的 ProtocolManager。
- [x] 实现 JSON 示例解析器及非法报文、质量状态、信号分发测试。
- [x] 将 ProtocolManager 接入通信 RX 链路；按设备显式绑定解析器，默认不解析。
- [x] 增加 BusinessDataManager，维护每台设备各变量的最新值，并阻止旧时间戳覆盖新值。
- [ ] 将业务变量持久化到独立历史表，并接入业务趋势页面。
- [ ] 真实设备协议明确后再实现对应解析器，不臆造寄存器地址、CAN ID 或报文格式。

### P2：报警闭环
- [x] 支持变量高低限阈值、启用/禁用和 Info/Warning/Critical 报警等级，并提供报警中心新增规则入口。
- [x] 报警规则和报警事件均持久化到 SQLite。
- [x] 实现触发、确认、恢复状态流转；只对 Good 质量数据进行阈值判断。
- [x] 报警中心展示事件并支持填写确认人。
- [x] 模拟设备生成独立业务变量，并接入 BusinessDataManager 与 AlarmManager；首次运行自动建立三台模拟设备的温度高限演示规则。
- [ ] 增加规则编辑/删除/启停管理、报警筛选和持续时间统计。

### P3：业务趋势与历史
- 实时曲线按业务变量绘制，而不只展示通信 FPS。
- 历史数据支持变量、设备、时间范围查询。
- 增加全量导出、数据保留策略和必要的索引/分页优化。

### P4：通信工程化与扩展
- 将阻塞或高频通信工作从 UI 线程隔离。
- 增加心跳、超时离线、可控重连和资源回收测试。
- 按真实需求增加 Modbus、MQTT、WebSocket。

### P5：产品化
- 登录、角色权限和操作审计。
- 插件接口与独立驱动插件。
- GitHub Actions 自动构建测试、安装包和部署文档。

## 当前边界与验收原则

- 模拟设备只用于演示和测试，不得影响真实设备。
- TX 统计代表本软件/驱动接受发送请求，不代表远端已收到。
- 未明确协议的设备只保存原始帧，不推断业务含义。
- GitHub 文件更新成功不代表用户本地 Qt 编译通过；每个阶段仍需本地构建和测试验收。


### 业务变量实时趋势（本阶段）

- [x] 在实时趋势页增加温度、转速、电流独立曲线页签。
- [x] 从 BusinessDataManager 读取统一业务数据，按设备筛选。
- [x] 展示最近 60 秒曲线及当前值、最大值、最小值、平均值。
- [ ] 增加业务变量历史持久化与历史时间范围查询。
