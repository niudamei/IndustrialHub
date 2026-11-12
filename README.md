# IndustrialHub

基于 Qt 6 Widgets + CMake 的工业设备管理上位机项目。

## 当前版本

- Qt：6.5 或更高版本
- C++：17
- 构建：CMake
- UI：Qt Designer（`.ui`）
- 当前数据存储：内存模拟数据（程序退出后不会保留）

## 已实现

### 设备管理

- 设备信息统一由 `DeviceManager` 管理，页面不再直接维护设备数据副本。
- 新增设备：设备编号、设备名称必填，设备编号不区分大小写且不可重复。
- 编辑设备：设备编号作为主键，编辑时只读；添加时间保留。
- 删除设备：二次确认。
- 查询设备：支持按设备编号、名称、类型、地址模糊搜索。
- 状态筛选：全部、在线、离线、故障。
- 状态更新：管理层提供按设备编号更新状态的接口。
- Qt signals/slots：设备增删改和状态变化后发出信号，页面自动刷新。

当前启动时会加载两条演示设备。后续接入 TCP/CAN/RS485 通信或数据库时，可以继续复用 `DeviceManager` 的接口。

## Windows 构建

建议使用 Qt Creator 打开项目根目录的 `CMakeLists.txt`，选择已安装的 Qt 6 MinGW 64-bit Kit，配置并构建运行。

也可以在已配置 Qt 6 CMake 环境的终端中执行：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_64"
cmake --build build --config Debug
```

请将示例路径中的 Qt 版本目录替换为本机实际安装路径。

## 目录说明

- `app/`：程序入口
- `src/pages/`：功能页面、设备数据结构和设备管理逻辑
- `src/ui/MainWindow/`：主窗口
- `CustomFolder/`：自定义控件

## 后续开发建议

1. 为设备数据增加 JSON 持久化，避免重启后演示数据重置。
2. 增加通信接口抽象层，将 TCP、UDP、CAN、CANFD、RS485 驱动与页面解耦。
3. 增加设备在线心跳、超时离线和故障状态联动。
4. 增加日志、报警和历史数据存储。
