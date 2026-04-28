/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/main.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
/**
 * main.cpp — SerialBox 入口
 */
#include <QApplication>
#include <QIcon>
#include "SerialBox/app/Application.h"

// 入口函数
int main(int argc, char *argv[])
{
    QApplication qapp(argc, argv);                // Qt 应用对象
    qapp.setApplicationName("SerialBox");         // 应用名称
    qapp.setApplicationVersion("1.0.0");          // 应用版本
    qapp.setOrganizationName("SerialBox");        // 组织名称
    qapp.setWindowIcon(QIcon(":/icons/app.svg")); // 应用图标

    Application app;
    if (!app.initialize())
    {
        return -1;
    }

    return qapp.exec();
}
