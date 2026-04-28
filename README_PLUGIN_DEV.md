# SerialBox 动态插件设计说明（兼容实现指南）

本文档面向第三方插件开发者，目标是让你只看这份文档就能实现与当前 SerialBox 兼容的动态插件。

## 1. 版本与兼容范围

- 宿主版本：当前仓库主分支（Qt6 + C++20）
- 插件接口版本：`SerialPlugin_IID = com.serialbox.ISerialPlugin/1.0`
- 动态库类型：Qt Plugin（`.dll/.so/.dylib`）
- 最低要求：
  - 继承 `ISerialPlugin`
  - 导出 Qt 插件元数据
  - 生命周期方法可被 `PluginManager` 正常调用

## 2. 宿主架构与插件位置

宿主初始化顺序（简化）：

1. `ConfigManager` 加载配置
2. `DataPipeline` 初始化
3. `SerialPortManager` 初始化
4. `PluginManager` 扫描并加载动态库
5. 主窗口 `MainWindow` 构建

动态插件目录：

- 来源：`ConfigManager::pluginDir()`
- 行为：`PluginManager::loadPlugins(pluginDir)` 会遍历目录下动态库并尝试加载

## 3. 必须实现的接口

接口定义文件：`include/SerialBox/plugin/ISerialPlugin.h`

### 3.1 元信息接口

必须实现：

- `MetaInfo metaInfo() const`

`MetaInfo` 字段：

- `name`：插件唯一展示名（建议唯一）
- `version`：版本号（例如 `1.0.0`）
- `author`：作者信息
- `description`：功能描述

### 3.2 生命周期接口

可重写方法：

- `bool init()`：初始化资源，失败返回 `false`
- `void onStart()`：启用/启动阶段回调
- `void onStop()`：禁用/停止阶段回调
- `void cleanup()`：卸载前清理

宿主调用时机：

- 加载成功后：`init()` -> `onStart()`
- 禁用插件：`onStop()`
- 重新启用：`onStart()`
- 卸载插件：`onStop()` -> `cleanup()` -> 卸载动态库

### 3.3 能力声明（可选）

- `hasDataHook()`
- `hasProtocolParser()`
- `hasUiPanel()`

说明：当前宿主主要用于能力标识，不会自动注入上下文对象。

## 4. Qt 插件导出规范（必须）

你的插件类必须包含：

- `Q_OBJECT`
- `Q_PLUGIN_METADATA(IID SerialPlugin_IID)`
- `Q_INTERFACES(ISerialPlugin)`

最小骨架示例：

```cpp
#include "SerialBox/plugin/ISerialPlugin.h"

class ExamplePlugin final : public ISerialPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SerialPlugin_IID)
    Q_INTERFACES(ISerialPlugin)

public:
    MetaInfo metaInfo() const override {
        return {"ExamplePlugin", "1.0.0", "YourName", "示例插件"};
    }

    bool init() override {
        // 初始化资源
        return true;
    }

    void onStart() override {
        // 启动逻辑
    }

    void onStop() override {
        // 停止逻辑
    }

    void cleanup() override {
        // 释放资源
    }
};
```

## 5. PluginManager 运行时接口（宿主侧）

定义文件：

- `include/SerialBox/plugin/PluginManager.h`
- `src/plugin/PluginManager.cpp`

### 5.1 加载与卸载

- `void loadPlugins(const QString &pluginDir)`
  - 扫描目录并加载动态库
- `bool loadPluginFile(const QString &filePath)`
  - 加载单个动态库
- `bool unloadPlugin(const QString &name)`
  - 卸载指定插件
- `void unloadAll()`
  - 卸载全部

### 5.2 查询

- `QStringList loadedPluginNames() const`
- `QStringList availablePluginFiles(const QString &pluginDir) const`
- `QList<ISerialPlugin *> plugins() const`
- `ISerialPlugin *pluginByName(const QString &name) const`
- `bool isPluginEnabled(const QString &name) const`

### 5.3 启停控制

- `void enablePlugin(const QString &name, bool enable)`
  - `enable=true` 时调用 `onStart()`
  - `enable=false` 时调用 `onStop()`

### 5.4 事件信号

- `pluginLoaded(name)`
- `pluginUnloaded(name)`
- `pluginError(name, error)`

## 6. 兼容插件开发建议

### 6.1 线程与阻塞

- 不要在 UI 线程执行重计算
- 大量解析请放工作线程
- `onStop()` 与 `cleanup()` 必须可重入、可重复调用

### 6.2 资源与句柄

- 所有定时器、线程、文件句柄在 `cleanup()` 中释放
- 不在析构之外依赖宿主释放你的资源

### 6.3 命名与冲突

- `metaInfo().name` 建议全局唯一
- 若重名，宿主按名称进行管理，可能覆盖已有项

### 6.4 UI 插件建议

- 推荐独立窗口：`QDialog + Qt::Window`
- 避免模态阻塞主界面串口收发
- 关闭窗口建议 `hide()` 而非销毁，提升交互连续性

## 7. 动态插件与串口数据接入说明

当前 `ISerialPlugin` 本身不强制提供串口管理器注入接口。

建议做法：

1. 插件内部提供可选槽函数，例如 `setSerialManager(QObject*)`
2. 由宿主扩展代码在加载后通过 `QMetaObject::invokeMethod` 注入
3. 插件内部再 `qobject_cast` 到目标类型并连接信号

注意：以上是可扩展方案，不是当前接口硬性要求。

## 8. CMake 打包模板

```cmake
cmake_minimum_required(VERSION 3.20)
project(ExamplePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(ExamplePlugin MODULE
    ExamplePlugin.h
    ExamplePlugin.cpp
)

target_include_directories(ExamplePlugin PRIVATE
    /path/to/SerialBox/include
)

target_link_libraries(ExamplePlugin PRIVATE Qt6::Core)
```

产物复制到 `pluginDir` 后即可被扫描加载。

## 9. 调试与验收清单

加载验收：

1. 动态库放入 `pluginDir`
2. 打开“插件管理器”执行扫描加载
3. 检查是否收到 `pluginLoaded`
4. 禁用/启用后观察 `onStop/onStart` 是否按预期触发
5. 卸载后确认 `cleanup` 已释放资源

错误排查：

- `pluginError(..., "不是有效的 SerialBox 插件")`
  - 检查 `Q_INTERFACES(ISerialPlugin)` 与 IID
- `pluginError(..., "插件初始化失败")`
  - 检查 `init()` 返回值与内部依赖
- 加载失败但无实例
  - 检查 Qt 版本、编译器 ABI、依赖库是否齐全

## 10. 兼容性结论

只要满足以下条件，即可被当前 SerialBox 动态插件系统识别并管理：

1. 正确继承 `ISerialPlugin`
2. 正确导出 Qt 插件元数据（IID 一致）
3. 生命周期方法可稳定执行
4. 动态库位于 `pluginDir` 且依赖完整
