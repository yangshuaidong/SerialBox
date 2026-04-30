cmake_minimum_required(VERSION 3.19)
project(SerialBox VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt6 依赖 ──
find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets SerialPort Network
)
# WebSockets 可选（守护进程 WebSocket 中继）
find_package(Qt6 QUIET COMPONENTS WebSockets)
if(Qt6WebSockets_FOUND)
    add_definitions(-DENABLE_WEBSOCKETS)
endif()

# ── pybind11（可选，Python 引擎） ──
option(ENABLE_PYTHON "Enable embedded Python engine" OFF)
if(ENABLE_PYTHON)
    find_package(pybind11 REQUIRED)
    add_definitions(-DENABLE_PYTHON)
endif()

# ── 源文件收集 ──
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/ConfigManager.cpp
    src/core/SerialPort.cpp
    src/core/SerialPortManager.cpp
    src/core/DataPipeline.cpp
    src/core/EncodingDetector.cpp
    src/core/BackPressureQueue.cpp
    src/protocol/ModbusParser.cpp
    src/protocol/JsonParser.cpp
    src/protocol/CustomFrameParser.cpp
    src/plugin/BerTesterPlugin.cpp
    src/plugin/TestReportPlugin.cpp
    src/plugin/AdvancedLabPlugin.cpp
    src/ui/MainWindow.cpp
    src/ui/ReceivePanel.cpp
    src/ui/SendPanel.cpp
    src/ui/SerialDialog.cpp
    src/ui/SerialSettings.cpp
    src/ui/CommandSidebar.cpp
    src/ui/WaveformWidget.cpp
    src/ui/ProtocolTreeView.cpp
    src/ui/WorkspaceModes.cpp
    src/ui/SearchBar.cpp
    src/ui/StatusBar.cpp
    src/daemon/SerialBridge.cpp
    src/daemon/JsonRpcServer.cpp
)

set(HEADERS
    include/SerialBox/app/Application.h
    include/SerialBox/app/ConfigManager.h
    include/SerialBox/core/ISerialTransport.h
    include/SerialBox/core/SerialPort.h
    include/SerialBox/core/SerialPortManager.h
    include/SerialBox/core/DataPipeline.h
    include/SerialBox/core/EncodingDetector.h
    include/SerialBox/core/BackPressureQueue.h
    include/SerialBox/protocol/IProtocolParser.h
    include/SerialBox/protocol/ModbusParser.h
    include/SerialBox/protocol/JsonParser.h
    include/SerialBox/protocol/CustomFrameParser.h
    include/SerialBox/plugin/ISerialPlugin.h
    include/SerialBox/plugin/BerTesterPlugin.h
    include/SerialBox/plugin/TestReportPlugin.h
    include/SerialBox/plugin/AdvancedLabPlugin.h
    include/SerialBox/ui/MainWindow.h
    include/SerialBox/ui/ReceivePanel.h
    include/SerialBox/ui/SendPanel.h
    include/SerialBox/ui/SerialDialog.h
    include/SerialBox/ui/SerialSettings.h
    include/SerialBox/ui/CommandSidebar.h
    include/SerialBox/ui/WaveformWidget.h
    include/SerialBox/ui/ProtocolTreeView.h
    include/SerialBox/ui/WorkspaceModes.h
    include/SerialBox/ui/SearchBar.h
    include/SerialBox/ui/StatusBar.h
    include/SerialBox/daemon/SerialBridge.h
    include/SerialBox/daemon/JsonRpcServer.h
)

# ── 资源 ──
set(RESOURCES
    resources/resources.qrc
)

# ── 构建目标 ──
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)


set_target_properties(SerialBox PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Network
)
if(Qt6WebSockets_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::WebSockets)
endif()

if(ENABLE_PYTHON)
    target_link_libraries(${PROJECT_NAME} PRIVATE pybind11::embed)
    target_sources(${PROJECT_NAME} PRIVATE src/python/PythonEngine.cpp src/python/ScriptRunner.cpp)
endif()

# ── 安装 ──
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
