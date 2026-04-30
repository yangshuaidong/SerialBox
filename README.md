# SerialBox

> 现代化跨平台串口调试工具 · Qt6 · C++20

![Build](https://img.shields.io/badge/Qt-6.5+-41CD52?logo=qt&logoColor=white)
![C++](https://img.shields.io/badge/C++-20-00599C?logo=cplusplus)
![Platform](https://img.shields.io/badge/平台-Windows%20|%20Linux%20|%20macOS-lightgrey)

---

## 🏗️ 架构总览

```
SerialBox/
├── CMakeLists.txt                 # CMake 构建（Qt6 + 可选 pybind11）
├── src/
│   ├── main.cpp                   # 入口
│   ├── app/                       # 应用生命周期 + 配置管理
│   ├── core/                      # 串口抽象层 / 数据管道 / 流控
│   ├── protocol/                  # Modbus / JSON / 自定义帧解析
│   ├── plugin/                    # 插件动态加载 .so/.dll
│   ├── python/                    # pybind11 嵌入式 Python 引擎
│   ├── ui/                        # Qt6 Widgets UI 层
│   └── daemon/                    # WebSocket 中继 + JSON-RPC 2.0
├── include/SerialBox/             # 头文件镜像
├── plugins/                       # 示例插件
├── scripts/                       # Python 脚本示例
└── resources/
    ├── icons/                     # SVG 图标
    ├── themes/dark.qss            # Catppuccin Mocha 暗色主题
    └── resources.qrc              # Qt 资源文件
```

## ✨ 功能矩阵

### 🔴 高优先级 — 核心功能

| 模块 | 功能 | 说明 |
|------|------|------|
| 基础通信 | 波特率/数据位/停止位/校验/流控 | 串口通信核心参数控制 |
| 基础通信 | 串口热插拔检测 | 设备拔插实时同步 UI |
| 基础通信 | 断线自动重连 | 可配重试策略，工业级调试保障 |
| 基础通信 | 大数据流控与背压队列 | 防高速数据流阻塞 UI 线程 |
| 数据收发 | 文本/HEX 双模式切换 | 基础收发能力 |
| 数据收发 | 智能搜索高亮 (正则/HEX/导航) | 快速定位关键指令与响应 |
| 数据管道 | DataPipeline 拦截与预处理钩子 | 插件/协议解析数据流转基座 |
| 高级发送 | 预设命令库 (分组/快捷键/变量替换) | 支持 `${timestamp}` 等动态占位 |
| 高级发送 | 响应等待与匹配校验 | 超时控制/正则/前缀匹配 |
| 高级发送 | Headless CLI 模式 | 无 GUI 运行，适配 CI/CD |
| 数据展示 | 虚拟列表优化 (10万+行) | 解决长日志内存与渲染卡顿 |
| 插件架构 | ISerialPlugin 接口 + 动态加载 | `.so/.dll` 生命周期管理 |
| 协议框架 | IProtocolParser + Modbus/JSON | 标准化 match/parse/build |
| 守护进程 | WebSocket 中继 + JSON-RPC | 多端口并发与事件订阅 |
| 基础 UI | 接收区 3/4 + 发送区 1/4 | Qt6 现代化暗色界面 |
| 配置管理 | ConfigManager 参数持久化 | 窗口布局/串口参数/插件空间隔离 |

### 🟠 中优先级

| 模块 | 功能 | 说明 |
|------|------|------|
| 基础通信 | 定时自动发送与收发字节统计 | 基础调试与流量监控 |
| 基础通信 | 编码自动检测 (UTF-8/GBK/ASCII) | 智能 fallback 防乱码 |
| 数据收发 | 接收时间戳、自动换行、发送回显 | 提升报文可读性 |
| 高级发送 | 命令序列编辑器 (条件/间隔/循环) | 复杂自动化测试流程编排 |
| 高级发送 | 文件分块发送 (大文件进度/流控) | 固件升级/批量数据下发 |
| 数据展示 | 实时波形图 (数值折线/缩放导出) | 传感器/ADC 数据可视化 |
| 数据展示 | 协议解析树 (JSON/Modbus 折叠) | 结构化报文直观查看 |
| 插件架构 | 示例插件与开发规范文档 | 降低第三方扩展门槛 |
| Python | 完整嵌入式 Python 环境 | pybind11 集成，开箱即用 |
| Python | 安全沙箱 + 核心 API 绑定 | SerialPort/Logger 零拷贝映射 |
| 守护进程 | 多端口并发 + 多客户端/Wasm | 测试报告 HTML 可视化 |
| 基础 UI | 多工作区模式 (新手/专家/自动化) | 布局与功能可见性动态切换 |

### 🟡 低优先级

| 模块 | 功能 | 说明 |
|------|------|------|
| 数据展示 | 智能日志分卷保存 | 按时间/大小自动切割文件 |
| 守护进程 | Jinja2 离线暗色模板 | 测试报告开箱即用 |
| 工程化 | Inno Setup 一键安装包 | 含 Python 运行时打包 |
| 未来兼容 | ISerialTransport HAL | 为 WebSerial/Wasm 预留接口 |

---

## 🔨 构建

### 前置依赖

- **Qt 6.5+** (Widgets, SerialPort, Network, WebSockets)
- **CMake 3.19+**
- **C++20 编译器** (GCC 12+ / MSVC 2022 / Clang 15+)
- **pybind11** (可选，启用 Python 引擎)

### 编译

```bash
# 基础构建（无 Python）
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 启用 Python 引擎
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_PYTHON=ON
cmake --build . -j$(nproc)
```

### VSCode 配置

`.vscode/settings.json`:
```json
{
    "cmake.sourceDirectory": "${workspaceFolder}",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureOnOpen": true,
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

`.vscode/launch.json`:
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "SerialBox Debug",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/SerialBox",
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "preLaunchTask": "CMake: build"
        }
    ]
}
```

---

## 🧩 插件开发

### 接口定义

```cpp
#include <SerialBox/plugin/ISerialPlugin.h>

class MyPlugin : public QObject, public ISerialPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SerialPlugin_IID)
    Q_INTERFACES(ISerialPlugin)

public:
    MetaInfo metaInfo() const override {
        return {"MyPlugin", "1.0.0", "Author", "示例插件"};
    }
    bool init() override { return true; }
    void onStart() override { /* 注册钩子 */ }
    void onStop() override { /* 清理 */ }
};
```

### CMakeLists.txt

```cmake
add_library(MyPlugin MODULE plugin.cpp)
target_link_libraries(MyPlugin PRIVATE Qt6::Core SerialBox)
```

---

## 🐍 Python 脚本示例

```python
# scripts/read_sensor.py
import serialbox as sb

port = sb.SerialPort()
port.open("COM3", baudrate=115200)

# 发送查询
port.write(b"AT+SENSOR?\r\n")

# 读取响应
response = port.read_until(b"\n", timeout=2)
print(f"传感器数据: {response.decode('utf-8')}")
```

---

## 📐 UI 布局

```
┌──────────────────────────────────────────────────────────────┐
│ 📁 文件  📡 串口  👁 视图  🔧 工具  ❓ 帮助     SerialBox  │ [基础|专家|自动化]
├──────────────────────────────────────────────────────────────┤
│ [●COM3@115200] [🔌] [🔄] [Aa|⬡] [🕐↩👁] [🔍🗑📌] [📨📈🌳🧩] │
├────────────────────────┬─────────────────────────────────────┤
│ 📥 接收区              │ 📨 命令库                           │
│ [ASCII][HEX][Modbus]   │ ┌─ 常用 AT 指令 ─────────────────┐ │
│ ┌──────────────────────┤ │  F1 查询版本    AT+VERSION?    │ │
│ │ 18:32:01.234 ◄ SYS..│ │  F2 查询传感    AT+SENSOR?    │ │
│ │ 18:32:02.001 ► AT+..│ │  F3 LED 开      AT+LED=ON     │ │
│ │ 18:32:04.000 ◄ 01.. │ ├─ Modbus 查询 ─────────────────┤ │
│ │ ...                  │ │  F6 读保持寄存器  01 03 ...    │ │
│ │      ( 3/4 高度 )    │ │  F7 读输入寄存器  01 04 ...    │ │
│ │                      │ ├─ 自定义 ──────────────────────┤ │
│ │                      │ │  F9 心跳包      PING_${ts}    │ │
│ └──────────────────────┘ └───────────────────────────────┘ │
│ ─────────────────────────────────────────────────────────── │
│ 📤 发送区                                                    │
│ ┌──────────────────────────────────────────────────────────┐ │
│ │ 输入要发送的数据...                                       │ │
│ └──────────────────────────────────────────────────────────┘ │
│ [追加\r\n|] Enter发送  [📂历史] [🚀 发送]                    │
├──────────────────────────────────────────────────────────────┤
│ 🔌COM3@115200 │ ●已连接 │ RX:1847 TX:326 │ 编码:UTF-8      │
└──────────────────────────────────────────────────────────────┘
```

---

## 📄 License

MIT
