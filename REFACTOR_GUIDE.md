# SerialBox 重构指南 v2.0

> 基于当前代码审计，参照 Qt/Google C++/ROS2 编程规范，系统性修正架构缺陷、协议逻辑、UI配色与配置对齐问题。

---

## 📋 问题总览（按严重程度分级）

### 🔴 P0 — 必须修复（逻辑错误 / 数据丢失）

| # | 模块 | 问题 | 文件 |
|---|------|------|------|
| 1 | JSON-RPC | `processRequest` 不处理 **notification**（id 缺失时返回含 `"id": null` 的响应，违反 JSON-RPC 2.0 规范） | `JsonRpcServer.cpp` |
| 2 | JSON-RPC | `broadcastEvent` 构造的通知缺少规范说明——JSON-RPC 2.0 通知 **不得有 `id` 字段**（当前正确），但接收端未区分 request/notification | `SerialBridge.cpp` |
| 3 | Modbus | `match()` 中 FC=0x10 (写多寄存器) 的 `expectedLen=8` 是错误的——请求帧至少 9+写字节数，且**未校验 CRC** 就返回 `match=true`，导致帧同步错误 | `ModbusParser.cpp` |
| 4 | Modbus | `parse()` 只处理 FC=0x03/0x04/0x06，**缺少 FC=0x01/0x02/0x05/0x0F/0x10** 的解析分支，大量 Modbus 帧静默丢弃 | `ModbusParser.cpp` |
| 5 | Modbus | `build()` 只支持读寄存器（0x03/0x04），**不支持写操作帧的构建** | `ModbusParser.cpp` |
| 6 | DataPipeline | `processIncoming` 钩子拦截数据（`data.isEmpty()`）后不发出任何信号，**UI 和插件完全不知道数据被吞了**——应改为发出 `dataIntercepted` 信号 | `DataPipeline.cpp` |
| 7 | CustomFrame | CRC 使用简单累加（`crc += byte`），**工业场景下检错能力极弱**，应至少支持 CRC-16/CCITT | `CustomFrameParser.cpp` |
| 8 | JsonParser | `match()` 只检查 `{}` 而不检查 JSON-RPC 的 `"jsonrpc": "2.0"` 字段，**无法区分普通 JSON 与 JSON-RPC 帧** | `JsonParser.cpp` |
| 9 | SerialBridge | `serial.send` RPC 只接受 hex 格式，**不支持 text/base64 发送**，与 UI 的文本/HEX 双模式脱节 | `SerialBridge.cpp` |
| 10 | MainWindow | `loadSettings()` 硬编码 `m_mainSplitter->setSizes({600, 200})`，**完全忽略** `ConfigManager::splitRatio()` | `MainWindow.cpp:645` |

### 🟠 P1 — 高优先级（架构规范 / 配置对齐）

| # | 模块 | 问题 | 文件 |
|---|------|------|------|
| 11 | DataPipeline | `setDisplayMode` / `setTimestampEnabled` / `setEchoEnabled` **不持久化到 ConfigManager**，重启后丢失 | `DataPipeline.cpp` |
| 12 | DataPipeline | DisplayMode 切换后 `processIncoming` 不重新处理已缓存数据（由 `ReceivePanel::refreshDisplayMode` 负责，但**缺少管道事件通知机制**） | 架构问题 |
| 13 | JsonRpcServer | Handler 签名 `QJsonValue(const QJsonObject&)` 无法返回错误码——所有异常都被吞成 `-32000`，**无法区分参数错误(-32602)和内部错误** | `JsonRpcServer.h` |
| 14 | MainWindow | `setupToolBar()` 没有同步 `m_timestampAction`/`m_echoAction` 的初始状态到 `DataPipeline`（pipeline 默认 true，action 默认 true，但**没有显式连接**） | `MainWindow.cpp` |
| 15 | ConfigManager | 不保存 `displayMode`、`timestampEnabled`、`echoEnabled`、`autoNewline`，这些是用户高频切换的设置 | `ConfigManager.cpp` |
| 16 | ReceivePanel | 搜索高亮使用**硬编码颜色** `#E8F1FB` / `#CFE2F8`，在暗色主题下几乎不可见 | `ReceivePanel.cpp` |
| 17 | ReceivePanel | 书签颜色 `#fff3b0` 同样硬编码，暗色主题下对比度不足 | `ReceivePanel.cpp` |
| 18 | SerialPort | `onErrorOccurred` 对比 `QSerialPort::ResourceError` 时使用**已废弃的 enum 值**（Qt 6.x 已标记 deprecated） | `SerialPort.cpp` |
| 19 | 全局 | `BEGINNER_NOTE` 注释散布在每个文件中，违反 Google C++ Style Guide 的"代码自解释"原则 | 全部 .cpp/.h |

### 🟡 P2 — 中优先级（UI/主题/体验）

| # | 模块 | 问题 | 文件 |
|---|------|------|------|
| 20 | 主题 | 所有 5 个 .qss 中 `QComboBox::down-arrow` 使用 `image: none` + CSS border hack，**Qt6 下渲染异常** | 所有 .qss |
| 21 | 主题 | `dark.qss` 的 `*` 通配符设背景色导致**所有弹窗/下拉菜单继承错误背景** | `dark.qss` |
| 22 | 主题 | 缺少 `QDialog`、`QMessageBox`、`QFileDialog` 的样式规则，弹窗样式与主界面割裂 | 所有 .qss |
| 23 | 主题 | `QPlainTextEdit` 在 dark 主题下 `selection-background-color: #7c3aed` 与文本色 `#cdd6f4` 对比度不够 WCAG AA | `dark.qss` |
| 24 | UI | MainWindow 工具栏 toggle 按钮**选中态文字不变**（`"时间戳"` → `"时间戳"`，没有 ✓ 前缀） | `MainWindow.cpp` |
| 25 | UI | `WorkspaceModes` 的 Mode 枚举在 `MainWindow::switchWorkMode` 中使用了 `static_cast` 但**没有 UI 切换按钮**（菜单栏/工具栏无入口） | `MainWindow.cpp` |

---

## 🔧 详细修复方案

### 1. JSON-RPC 2.0 协议合规修复

**问题**: `JsonRpcServer::processRequest` 不区分 Request 和 Notification。

**JSON-RPC 2.0 规范要点**:
- Request: 必须有 `"id"` 字段（非 null）
- Notification: `"id"` 字段**不存在**，服务端**不得回复**
- 当前代码：notification 的 `id` 解析为 `QJsonValue::Undefined`，仍返回响应

**修复方案**:

```cpp
// JsonRpcServer.h — 新增返回类型枚举
enum class MessageType { Request, Notification, Invalid };

struct ProcessOutput {
    MessageType type = MessageType::Invalid;
    QJsonObject response;  // 仅当 type == Request 时有效
};

ProcessOutput processMessage(const QJsonObject &msg);

// JsonRpcServer.cpp
JsonRpcServer::ProcessOutput JsonRpcServer::processMessage(const QJsonObject &msg)
{
    ProcessOutput out;
    QString method = msg.value("method").toString();
    QJsonValue id = msg.value("id");
    bool isNotification = !msg.contains("id");

    if (method.isEmpty()) {
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeError(id, -32600, "Invalid Request: missing method");
        }
        return out;
    }

    auto it = m_handlers.constFind(method);
    if (it == m_handlers.constEnd()) {
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeError(id, -32601, QString("Method not found: %1").arg(method));
        }
        return out;
    }

    QJsonObject params = msg.value("params").toObject();
    try {
        QJsonValue result = it.value()(params);
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeResponse(id, result);
        } else {
            out.type = MessageType::Notification;
        }
    } catch (const std::exception &e) {
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeError(id, -32000, QString("Internal error: %1").arg(e.what()));
        }
    }
    return out;
}
```

**SerialBridge 对应修改**:

```cpp
void SerialBridge::onTextMessage(QWebSocket *sender, const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        auto err = JsonRpcServer::makeError(QJsonValue(), -32700, "Parse error");
        sender->sendTextMessage(QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }

    auto output = m_rpcServer.processMessage(doc.object());
    if (output.type == JsonRpcServer::MessageType::Request) {
        sender->sendTextMessage(
            QJsonDocument(output.response).toJson(QJsonDocument::Compact));
    }
    // Notification: 不回复
}
```

---

### 2. Modbus 协议解析全面修复

**问题**: CRC 不校验、功能码覆盖不全、match 逻辑错误。

**修复方案 — 完整重写 `ModbusParser`**:

```cpp
// ModbusParser.h — 新增
enum class FunctionCategory { ReadCoils, ReadInputs, ReadHolding, ReadInputRegs,
                               WriteSingleCoil, WriteSingleReg,
                               WriteMultiCoils, WriteMultiRegs, Unknown };

// ModbusParser.cpp 核心逻辑
bool ModbusParser::match(const QByteArray &buffer)
{
    if (buffer.size() < 4) return false; // 最小: ID(1) + FC(1) + CRC(2)

    quint8 fc = static_cast<quint8>(buffer[1]);
    int expectedLen = expectedFrameLength(buffer);
    if (expectedLen < 0) return false;
    if (buffer.size() < expectedLen) return false;

    // ★ 关键修复：CRC 校验
    quint16 receivedCrc = static_cast<quint8>(buffer[expectedLen - 2])
                        | (static_cast<quint8>(buffer[expectedLen - 1]) << 8);
    quint16 calcCrc = crc16(buffer.left(expectedLen - 2));
    return receivedCrc == calcCrc;
}

int ModbusParser::expectedFrameLength(const QByteArray &buffer) const
{
    if (buffer.size() < 2) return -1;
    quint8 fc = static_cast<quint8>(buffer[1]);

    // 响应帧 (0x03/0x04): [ID][FC][ByteCount][Data...][CRC]
    if (fc == 0x03 || fc == 0x04) {
        if (buffer.size() < 3) return -1;
        int byteCount = static_cast<quint8>(buffer[2]);
        return 3 + byteCount + 2;
    }
    // 请求帧 (0x03/0x04): [ID][FC][AddrH][AddrL][CountH][CountL][CRC]
    // 注意：这里需要通过上下文判断是请求还是响应
    // 实际做法：FC 高位为 0 表示请求，0x80 表示异常响应

    // 写单寄存器/线圈 0x05/0x06: 回显 = 8 字节
    if (fc == 0x05 || fc == 0x06) return 8;

    // 写多寄存器 0x10 请求: [ID][FC][AddrH][AddrL][QtyH][QtyL][ByteCount][Data...][CRC]
    if (fc == 0x10) {
        if (buffer.size() < 7) return -1;
        int byteCount = static_cast<quint8>(buffer[6]);
        return 7 + byteCount + 2;
    }
    // 0x10 响应: 8 字节
    // 需要上下文判断

    // 写多线圈 0x0F: 同上逻辑
    if (fc == 0x0F) {
        if (buffer.size() < 7) return -1;
        int byteCount = static_cast<quint8>(buffer[6]);
        return 7 + byteCount + 2;
    }

    // 异常响应: [ID][FC|0x80][ExceptionCode][CRC] = 5 字节
    if (fc & 0x80) return 5;

    // 读请求 (0x01/0x02/0x03/0x04): 8 字节
    if (fc >= 0x01 && fc <= 0x04) return 8;

    return -1; // 未知
}
```

---

### 3. DataPipeline 配置对齐

**问题**: 显示模式、时间戳、回显等设置不持久化。

**修复方案**:

```cpp
// ConfigManager.h — 新增
struct DisplaySettings {
    int displayMode = 0;       // 0=Text, 1=Hex
    bool timestampEnabled = true;
    bool autoNewline = true;
    bool echoEnabled = true;
};
DisplaySettings displaySettings() const;
void setDisplaySettings(const DisplaySettings &s);

// ConfigManager.cpp
ConfigManager::DisplaySettings ConfigManager::displaySettings() const {
    DisplaySettings s;
    s.displayMode = m_settings.value("display/mode", 0).toInt();
    s.timestampEnabled = m_settings.value("display/timestamp", true).toBool();
    s.autoNewline = m_settings.value("display/autoNewline", true).toBool();
    s.echoEnabled = m_settings.value("display/echo", true).toBool();
    return s;
}
void ConfigManager::setDisplaySettings(const DisplaySettings &s) {
    m_settings.setValue("display/mode", s.displayMode);
    m_settings.setValue("display/timestamp", s.timestampEnabled);
    m_settings.setValue("display/autoNewline", s.autoNewline);
    m_settings.setValue("display/echo", s.echoEnabled);
}

// MainWindow::setManagers 中同步初始状态
void MainWindow::syncPipelineFromConfig()
{
    if (!m_config || !m_pipeline) return;
    auto ds = m_config->displaySettings();
    m_pipeline->setDisplayMode(static_cast<DataPipeline::DisplayMode>(ds.displayMode));
    m_pipeline->setTimestampEnabled(ds.timestampEnabled);
    m_pipeline->setAutoNewline(ds.autoNewline);
    m_pipeline->setEchoEnabled(ds.echoEnabled);

    // 同步 toolbar action 状态
    m_timestampAction->setChecked(ds.timestampEnabled);
    m_echoAction->setChecked(ds.echoEnabled);
    if (ds.displayMode == 1) {
        m_textModeAction->setChecked(false);
        m_hexModeAction->setChecked(true);
    }
}
```

---

### 4. 接收区搜索/书签暗色主题适配

**问题**: 硬编码浅色高亮色。

**修复方案 — 使用主题感知颜色**:

```cpp
// ReceivePanel.cpp — 替换硬编码颜色
namespace {
    QColor searchHighlightColor(bool isActive, const QPalette &pal) {
        bool dark = pal.base().color().lightness() < 128;
        if (dark) {
            return isActive ? QColor("#5B3A29") : QColor("#3D2B1F");  // 暗琥珀
        }
        return isActive ? QColor("#CFE2F8") : QColor("#E8F1FB");       // 浅蓝
    }

    QColor bookmarkColor(const QPalette &pal) {
        bool dark = pal.base().color().lightness() < 128;
        return dark ? QColor("#4A4520") : QColor("#fff3b0");  // 暗金 vs 浅黄
    }
}

// 替换所有 search() / searchNext() / searchPrev() / clearSearchHighlight()
// 中的硬编码颜色为函数调用
```

---

### 5. 主题 QSS 修复

**问题清单**:
1. `*` 通配符污染所有控件背景
2. 缺少 `QDialog`/`QMessageBox` 样式
3. `QComboBox::down-arrow` 在 Qt6 下失效
4. WCAG 对比度不达标

**dark.qss 修复片段**:

```css
/* ❌ 移除 * 通配符对背景的设置 */
* {
    color: #cdd6f4;
    font-family: 'Segoe UI', 'PingFang SC', 'Microsoft YaHei', system-ui, sans-serif;
    font-size: 13px;
}

/* ✅ 分别设置各组件背景 */
QMainWindow, QWidget#centralWidget {
    background-color: #1e1e2e;
}

/* ✅ Qt6 兼容下拉箭头 */
QComboBox::down-arrow {
    image: url(:/icons/chevron-down.svg);
    width: 12px;
    height: 12px;
}

/* ✅ 弹窗样式 */
QDialog {
    background-color: #1e1e2e;
    border: 1px solid #3a3a55;
    border-radius: 8px;
}

QMessageBox {
    background-color: #1e1e2e;
}
QMessageBox QLabel {
    color: #cdd6f4;
}

/* ✅ WCAG AA 对比度修正 — 选中背景 */
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border-color: #7c3aed;
    selection-background-color: #6D28D9;  /* 加深紫色 */
}
```

---

### 6. 工具栏 Toggle 状态可视化

**问题**: 开关按钮文字不变，用户无法感知当前状态。

**修复方案**:

```cpp
// MainWindow.cpp — 修改 toggle action 的 text 更新逻辑
m_timestampAction->setText(on ? "🕐 时间戳" : "⏱ 时间戳");
// 或使用 ✓ 前缀
m_timestampAction->setText(on ? "✓ 时间戳" : "时间戳");

m_echoAction->setText(on ? "✓ 回显" : "回显");

// 统一所有 toggle 使用相同的视觉约定
// 建议：所有 checkable action 选中时加 ✓ 前缀
```

---

### 7. JSON-RPC `serial.send` 支持多格式

**当前**: 只接受 hex 字符串。
**修复**: 支持 `hex`/`text`/`base64` 三种格式。

```cpp
m_rpcServer.registerMethod("serial.send", [this](const QJsonObject &params) {
    if (!m_serialManager || !m_serialManager->isConnected())
        return QJsonValue(QJsonObject{{"error", "not connected"}});

    QString format = params.value("format").toString("hex");
    QString payload = params.value("data").toString();
    QByteArray data;

    if (format == "hex") {
        data = QByteArray::fromHex(payload.remove(' ').toLatin1());
    } else if (format == "text") {
        data = payload.toUtf8();
    } else if (format == "base64") {
        data = QByteArray::fromBase64(payload.toLatin1());
    } else {
        return QJsonValue(QJsonObject{{"error", "unknown format"}});
    }

    qint64 written = m_serialManager->sendData(data);
    return QJsonValue(QJsonObject{
        {"written", static_cast<int>(written)},
        {"format", format}
    });
});
```

---

### 8. MainWindow splitRatio 配置丢失修复

```cpp
void MainWindow::loadSettings()
{
    if (!m_config) return;
    restoreGeometry(m_config->windowGeometry());
    restoreState(m_config->windowState());

    // ✅ 使用配置中的比例
    double ratio = m_config->splitRatio();
    int total = m_mainSplitter->height() > 0 ? m_mainSplitter->height() : 800;
    int recvH = static_cast<int>(total * ratio);
    int sendH = total - recvH;
    m_mainSplitter->setSizes({recvH, sendH});
}

void MainWindow::saveSettings()
{
    if (!m_config) return;
    m_config->setWindowGeometry(saveGeometry());
    m_config->setWindowState(saveState());
    auto sizes = m_mainSplitter->sizes();
    if (sizes.size() == 2 && sizes[0] + sizes[1] > 0) {
        m_config->setSplitRatio(
            static_cast<double>(sizes[0]) / (sizes[0] + sizes[1]));
    }
    // ✅ 保存 display settings
    DisplaySettings ds;
    ds.displayMode = static_cast<int>(m_pipeline->displayMode());
    ds.timestampEnabled = m_pipeline->timestampEnabled();
    ds.autoNewline = m_pipeline->autoNewline();
    ds.echoEnabled = m_pipeline->echoEnabled();
    m_config->setDisplaySettings(ds);
    m_config->save();
}
```

---

## 🏗️ 架构级改进建议

### A. 去除 BEGINNER_NOTE 注释

所有 `.cpp` / `.h` 文件顶部的 `BEGINNER_NOTE` 注释块违反 Google C++ Style Guide 的「代码自解释」原则。建议：

- ✅ 删除所有 `BEGINNER_NOTE` 注释
- ✅ 保留有意义的类/函数文档注释（Doxygen 格式）
- ✅ 将教学文档移至独立的 `docs/ARCHITECTURE.md`

### B. 引入日志框架

当前全项目使用 `qWarning()` 和直接 `QMessageBox`，建议：

```cpp
// 统一日志宏
#define SB_LOG_DEBUG(category, msg) \
    QCATEGORY(category).debug() << msg
#define SB_LOG_INFO(category, msg) \
    QCATEGORY(category).info() << msg
#define SB_LOG_WARN(category, msg) \
    QCATEGORY(category).warning() << msg

// 使用
Q_LOGGING_CATEGORY(lcSerial, "serialbox.serial")
Q_LOGGING_CATEGORY(lcProtocol, "serialbox.protocol")
Q_LOGGING_CATEGORY(lcRpc, "serialbox.rpc")

// 替换所有 qWarning("...") → SB_LOG_WARN(lcSerial, "...")
```

### C. 错误码体系

为 JSON-RPC 和内部错误建立统一错误码：

```cpp
namespace ErrorCodes {
    // JSON-RPC 标准
    constexpr int ParseError     = -32700;
    constexpr int InvalidRequest = -32600;
    constexpr int MethodNotFound = -32601;
    constexpr int InvalidParams  = -32602;
    constexpr int InternalError  = -32603;

    // SerialBox 扩展 (-32000 ~ -32099)
    constexpr int PortNotOpen     = -32001;
    constexpr int PortBusy        = -32002;
    constexpr int WriteFailed     = -32003;
    constexpr int InvalidHex      = -32004;
    constexpr int CrcMismatch     = -32005;
    constexpr int FrameTimeout    = -32006;
}
```

### D. DataPipeline → ConfigManager 事件桥

当前 `DataPipeline` 的配置变更（displayMode 等）不通知 `ConfigManager`，反之亦然。建议：

```cpp
// 方案：DataPipeline 暴露配置变更信号
signals:
    void displaySettingsChanged();  // displayMode/timestamp/echo 任一变化

// MainWindow 连接 → 自动持久化
connect(m_pipeline, &DataPipeline::displaySettingsChanged,
        this, &MainWindow::saveDisplaySettings);
```

### E. WorkspaceModes UI 入口

当前 `switchWorkMode()` 有逻辑但**无 UI 入口**。建议在工具栏右侧添加：

```cpp
// MainWindow::setupToolBar() 末尾
auto *modeCombo = new QComboBox(this);
modeCombo->addItems({"基础模式", "专家模式", "自动化模式"});
modeCombo->setCurrentIndex(static_cast<int>(m_config->workMode()));
connect(modeCombo, &QComboBox::currentIndexChanged,
        this, &MainWindow::switchWorkMode);
tb->addWidget(modeCombo);
```

---

## 📁 推荐的文件变更清单

```
修改文件                                    变更类型
─────────────────────────────────────────────────
include/SerialBox/daemon/JsonRpcServer.h    → 新增 MessageType 枚举 + processMessage
src/daemon/JsonRpcServer.cpp                → 重写请求处理，区分 notification
src/daemon/SerialBridge.cpp                 → 适配 processMessage + serial.send 多格式
include/SerialBox/protocol/ModbusParser.h   → 新增 expectedFrameLength + 完整 FC 枚举
src/protocol/ModbusParser.cpp               → 重写 match/parse/build，加入 CRC 校验
src/protocol/CustomFrameParser.cpp          → CRC-16/CCITT 替换简单累加
src/protocol/JsonParser.cpp                 → match 增加 JSON-RPC 2.0 识别
include/SerialBox/core/DataPipeline.h       → 新增 displaySettingsChanged 信号
src/core/DataPipeline.cpp                   → 新增 dataIntercepted 信号 + 配置联动
include/SerialBox/app/ConfigManager.h       → 新增 DisplaySettings 结构体
src/app/ConfigManager.cpp                   → 新增 displaySettings 读写
src/ui/MainWindow.cpp                       → 修复 splitRatio / toggle 文字 / 模式入口
src/ui/ReceivePanel.cpp                     → 搜索高亮暗色适配
resources/themes/dark.qss                   → 移除 * 通配符 / 弹窗样式 / 对比度
resources/themes/light.qss                  → 对应检查
resources/themes/ocean.qss                  → 对应检查
resources/themes/graphite.qss               → 对应检查
resources/themes/professional.qss           → 对应检查
src/core/SerialPort.cpp                     → 废弃 enum 替换
全部 .cpp/.h                                → 删除 BEGINNER_NOTE
```

---

## ✅ 执行顺序建议

```
Phase 1 — 协议层修复（1-2天）
  ├── Modbus CRC 校验 + 全功能码
  ├── JSON-RPC notification 处理
  ├── CustomFrame CRC 升级
  └── JsonParser JSON-RPC 识别

Phase 2 — 配置对齐（1天）
  ├── ConfigManager DisplaySettings
  ├── DataPipeline ↔ ConfigManager 桥接
  └── MainWindow splitRatio 修复

Phase 3 — UI/主题（1-2天）
  ├── 搜索/书签暗色适配
  ├── QSS * 通配符移除 + 弹窗样式
  ├── Toggle 状态可视化
  └── WorkspaceModes UI 入口

Phase 4 — 清理（0.5天）
  ├── 删除 BEGINNER_NOTE
  ├── 统一日志宏
  └── 错误码常量
```

---

*Generated: 2026-04-30 | SerialBox v1.0.0 Audit Report*
