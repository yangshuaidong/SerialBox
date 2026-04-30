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
