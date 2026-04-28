#include "SerialBox/plugin/CrcCheckerPlugin.h"
#include "SerialBox/core/SerialPortManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QCheckBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDateTime>

CrcCheckerPlugin::CrcCheckerPlugin(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void CrcCheckerPlugin::setSerialManager(SerialPortManager *manager)
{
    if (m_rxConn)
    {
        QObject::disconnect(m_rxConn);
    }

    m_serialManager = manager;
    if (!m_serialManager)
    {
        return;
    }

    m_rxConn = connect(m_serialManager, &SerialPortManager::dataReceived,
                       this, &CrcCheckerPlugin::onDataReceived);
}

void CrcCheckerPlugin::setupUi()
{
    setWindowTitle("插件: CRC校验");
    setMinimumSize(620, 420);
    setWindowFlags(Qt::Window);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    m_algoCombo = new QComboBox(this);
    m_algoCombo->addItems({"CRC-16/MODBUS", "CRC-16/CCITT-FALSE"});

    m_endianCombo = new QComboBox(this);
    m_endianCombo->addItems({"尾部CRC低字节在前", "尾部CRC高字节在前"});

    m_monitorCheck = new QCheckBox("启动后自动校验收到数据并写入接收区提示", this);
    m_monitorCheck->setChecked(true);

    form->addRow("算法", m_algoCombo);
    form->addRow("尾部CRC字节序", m_endianCombo);
    form->addRow(m_monitorCheck);
    layout->addLayout(form);

    auto *calcTitle = new QLabel("CRC计算与发送", this);
    calcTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(calcTitle);

    auto *calcRow = new QHBoxLayout();
    m_inputEdit = new QLineEdit(this);
    m_inputEdit->setPlaceholderText("输入HEX数据，如: 01 03 00 00 00 01");
    auto *calcBtn = new QPushButton("计算CRC", this);
    auto *sendBtn = new QPushButton("追加CRC并发送", this);
    calcRow->addWidget(m_inputEdit, 1);
    calcRow->addWidget(calcBtn);
    calcRow->addWidget(sendBtn);
    layout->addLayout(calcRow);

    m_crcLabel = new QLabel("CRC: --", this);
    layout->addWidget(m_crcLabel);

    auto *resultTitle = new QLabel("校验结果", this);
    resultTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(resultTitle);

    m_resultList = new QListWidget(this);
    layout->addWidget(m_resultList, 1);

    connect(calcBtn, &QPushButton::clicked, this, [this]()
            {
        bool ok = false;
        const QByteArray raw = parseHex(m_inputEdit->text(), &ok);
        if (!ok || raw.isEmpty()) {
            QMessageBox::warning(this, "CRC", "请输入合法HEX数据。");
            return;
        }
        const quint16 crc = calcCrc(raw);
        m_crcLabel->setText(QString("CRC: 0x%1").arg(crc, 4, 16, QChar('0')).toUpper()); });

    connect(sendBtn, &QPushButton::clicked, this, [this]()
            {
        if (!m_serialManager || !m_serialManager->isConnected()) {
            QMessageBox::information(this, "CRC", "请先连接串口。");
            return;
        }
        bool ok = false;
        QByteArray raw = parseHex(m_inputEdit->text(), &ok);
        if (!ok || raw.isEmpty()) {
            QMessageBox::warning(this, "CRC", "请输入合法HEX数据。");
            return;
        }
        const quint16 crc = calcCrc(raw);
        if (m_endianCombo && m_endianCombo->currentIndex() == 0) {
            raw.append(static_cast<char>(crc & 0xFF));
            raw.append(static_cast<char>((crc >> 8) & 0xFF));
        } else {
            raw.append(static_cast<char>((crc >> 8) & 0xFF));
            raw.append(static_cast<char>(crc & 0xFF));
        }
        m_serialManager->sendData(raw);
        m_resultList->addItem(QString("发送: %1").arg(QString(raw.toHex(' ').toUpper()))); });
}

void CrcCheckerPlugin::onDataReceived(const QByteArray &data)
{
    if (!m_monitorCheck || !m_monitorCheck->isChecked() || data.size() < 3)
    {
        return;
    }

    const QByteArray payload = data.left(data.size() - 2);
    const quint16 calc = calcCrc(payload);
    quint16 recv = 0;

    const quint8 b1 = static_cast<quint8>(data.at(data.size() - 2));
    const quint8 b2 = static_cast<quint8>(data.at(data.size() - 1));
    if (m_endianCombo && m_endianCombo->currentIndex() == 0)
    {
        recv = static_cast<quint16>(b1 | (b2 << 8));
    }
    else
    {
        recv = static_cast<quint16>((b1 << 8) | b2);
    }

    const bool pass = (calc == recv);
    const QString msg = QString("CRC %1 | recv=0x%2 calc=0x%3 | data=%4")
                            .arg(pass ? "PASS" : "FAIL")
                            .arg(recv, 4, 16, QChar('0'))
                            .arg(calc, 4, 16, QChar('0'))
                            .arg(QString(data.toHex(' ').toUpper()))
                            .toUpper();

    m_resultList->addItem(QString("[%1] %2")
                              .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), msg));
    emit crcResultReady(msg, pass);
}

quint16 CrcCheckerPlugin::calcCrc(const QByteArray &data) const
{
    if (m_algoCombo && m_algoCombo->currentIndex() == 1)
    {
        quint16 crc = 0xFFFF;
        for (unsigned char byte : data)
        {
            crc ^= static_cast<quint16>(byte) << 8;
            for (int i = 0; i < 8; ++i)
            {
                if (crc & 0x8000)
                {
                    crc = static_cast<quint16>((crc << 1) ^ 0x1021);
                }
                else
                {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }

    quint16 crc = 0xFFFF;
    for (unsigned char byte : data)
    {
        crc ^= byte;
        for (int i = 0; i < 8; ++i)
        {
            if (crc & 0x0001)
            {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

QByteArray CrcCheckerPlugin::parseHex(const QString &text, bool *ok) const
{
    if (ok)
        *ok = false;
    QString normalized = text;
    normalized.replace(QRegularExpression("[\\r\\n,;]+"), " ");
    const QStringList parts = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return {};
    }

    QByteArray out;
    out.reserve(parts.size());
    for (const QString &p : parts)
    {
        bool localOk = false;
        int v = p.toInt(&localOk, 16);
        if (!localOk || v < 0 || v > 255)
        {
            return {};
        }
        out.append(static_cast<char>(v));
    }
    if (ok)
        *ok = true;
    return out;
}
