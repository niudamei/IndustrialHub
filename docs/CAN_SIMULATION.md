# CAN / CAN-FD 离线模拟与联调方案

本文说明 IndustrialHub 在没有实体 CAN 盒子时如何进行分层测试，以及后续接入独立虚拟 CAN 总线的方案。

## 1. 先区分三种模拟层级

| 层级 | 方式 | 能验证什么 | 不能验证什么 |
|---|---|---|---|
| 应用内模拟 | IndustrialHub 的 SIMULATED / MODBUS-SIM | UI、收发链路、业务数据、报警、历史记录 | CAN 控制器、总线仲裁、物理层 |
| 虚拟 CAN 总线 | Qt SerialBus VirtualCAN 或其他虚拟总线 | 独立进程间 CAN/CAN-FD 帧交换、驱动适配、报文解析 | 真实电气层、终端电阻、ACK/错误帧和时序边界 |
| 实体硬件台架 | CANable、PCAN、Kvaser、Vector 等 | 实际控制器、收发器、线束、终端电阻和真实总线行为 | 仍需真实设备或 ECU 提供业务响应 |

虚拟 CAN 不是虚拟串口。虚拟串口模拟 COM 端点；虚拟 CAN 模拟 CAN 总线节点和帧分发，两者的 API、帧格式和驱动模型不同。

## 2. 推荐的软件工具

### Qt SerialBus VirtualCAN（优先考虑集成）

Qt SerialBus 提供 virtualcan 插件，可以通过本机 TCP/IP 连接多个 CAN 应用，不需要 CAN 硬件或厂商 DLL。默认 TCP 端口为 35468，也可通过完整 URL 指定端口。一个客户端发送的帧会分发给其他连接客户端。

官方说明：https://doc.qt.io/qt-6/qtserialbus-virtualcan-overview.html

注意：
- 需要安装与当前 Qt Kit 匹配的 Qt SerialBus 开发模块及 VirtualCAN 插件。
- IndustrialHub 当前 CAN/CAN-FD 实体适配器走 CANable SLCAN 串口协议；VirtualCAN 是另一种后端，不能把它当作 COM 口。
- 接入时应保留现有 CAN / CANFD / SLCAN 驱动，不覆盖真实硬件路径。
- VirtualCAN 是软件帧交换，不等价于真实总线仲裁、ACK、错误帧和物理层验证。

### EcuBus-Pro Simulate

EcuBus-Pro 提供无需适配器和厂商 DLL 的软件虚拟 CAN 总线，支持 CAN 与 CAN-FD，可用于多节点脚本和 ECU/测试端协作。

说明：https://app.whyengineer.com/docs/um/can/simulate.html

适合需要独立 CAN 工具、脚本和报文追踪的场景。与 IndustrialHub 的连接方式取决于所选工具版本及其暴露的接口，不能默认它会提供 SLCAN 虚拟 COM 口。

### Linux SocketCAN vcan

Linux 内核提供 vcan 虚拟网络接口，不需要 CAN 控制器即可在本机收发 CAN 帧。适合后续在 Linux CI 或开发机验证 SocketCAN 驱动。

官方说明：https://docs.kernel.org/networking/can.html

示例：
~~~bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
ip link show vcan0
~~~

vcan 是 Linux 网络接口，不是 Windows COM 口；Windows 原生 Qt 应用不能直接把它当作本机设备使用。

## 3. IndustrialHub 的建议接入结构

保持现有通信抽象，不修改实体 CAN 的 SLCAN 语义：

- CAN：现有 CANable SLCAN 经典 CAN。
- CANFD：现有 CANable SLCAN CAN-FD。
- VIRTUALCAN：新增独立虚拟总线后端，使用 Qt SerialBus virtualcan 插件。
- SIMULATED：继续作为应用内通用模拟通信。
- MODBUS-SIM：继续作为 Modbus RTU 离线设备模拟。

虚拟后端应把统一内部帧结构转换为 QCanBusFrame，RX 再转换回项目内部帧格式；必须覆盖标准/扩展 ID、经典 CAN/CAN-FD、DLC/载荷长度、BRS、远程帧限制和非法帧处理。Qt SerialBus 模块应作为可选构建依赖，缺失时不能影响现有 SLCAN 和其他通信类型。

## 4. 建议的验收矩阵

1. 同一台电脑启动两个独立 CAN 客户端，A 发帧、B 收帧；确认发送端不把自己的 TX 错认成远端 RX。
2. 验证标准帧、扩展帧、0–8 字节经典 CAN 载荷。
3. 验证 CAN-FD 载荷长度、BRS 和最大 64 字节边界。
4. 验证非法 ID、非法 DLC、空帧和不支持的帧类型会被拒绝并记录错误。
5. 验证虚拟总线断开、服务端退出、重连和设备释放不会遗留线程或 socket。
6. 验证虚拟 CAN 收到的原始帧可以继续进入现有诊断、历史和协议解析链路。
7. 最后使用 CANable / 实体 ECU 做物理层和真实设备联调。

## 5. 安全提醒

虚拟 CAN 只适用于开发、演示和自动化测试。涉及电机、执行器或车辆控制时，先在离线虚拟环境验证；接入真实设备前确认 ID、周期、波特率、控制权限和急停机制。虚拟总线通过不代表真实总线或设备安全验证通过。