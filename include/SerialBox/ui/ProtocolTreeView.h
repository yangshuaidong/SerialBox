#pragma once
#include <QWidget>

/**
 * ProtocolTreeView — 协议解析树 (JSON/Modbus 折叠展示)
 */
class ProtocolTreeView : public QWidget
{
    Q_OBJECT

public:
    explicit ProtocolTreeView(QWidget *parent = nullptr);

public slots:
    void setProtocolData(const QVariantMap &data, const QString &protocolName);

private:
    void setupUi();
};
