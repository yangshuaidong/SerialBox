#include "SerialBox/plugin/TestReportPlugin.h"
#include "SerialBox/core/SerialPortManager.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QTextStream>
#include <QMessageBox>

TestReportPlugin::TestReportPlugin(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void TestReportPlugin::setSerialManager(SerialPortManager *manager)
{
    m_serialManager = manager;
}

void TestReportPlugin::setupUi()
{
    setWindowTitle("插件: 测试报告");
    setMinimumSize(520, 360);
    setWindowFlags(Qt::Window);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setText("SerialBox 测试报告");

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems({"TXT", "JSON", "CSV"});

    form->addRow("标题", m_titleEdit);
    form->addRow("导出格式", m_formatCombo);
    layout->addLayout(form);

    m_includeTimestamp = new QCheckBox("包含导出时间", this);
    m_includePort = new QCheckBox("包含端口信息", this);
    m_includeTraffic = new QCheckBox("包含收发统计", this);
    m_includeUptime = new QCheckBox("包含连接时长", this);

    m_includeTimestamp->setChecked(true);
    m_includePort->setChecked(true);
    m_includeTraffic->setChecked(true);
    m_includeUptime->setChecked(true);

    layout->addWidget(m_includeTimestamp);
    layout->addWidget(m_includePort);
    layout->addWidget(m_includeTraffic);
    layout->addWidget(m_includeUptime);

    auto *btnRow = new QHBoxLayout();
    auto *exportBtn = new QPushButton("导出报告", this);
    auto *closeBtn = new QPushButton("关闭", this);
    btnRow->addStretch();
    btnRow->addWidget(exportBtn);
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    connect(exportBtn, &QPushButton::clicked, this, &TestReportPlugin::exportReport);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void TestReportPlugin::exportReport()
{
    if (!m_serialManager)
    {
        QMessageBox::warning(this, "测试报告", "串口管理器不可用。");
        return;
    }

    const QString fmt = m_formatCombo ? m_formatCombo->currentText() : QString("TXT");
    const QString ext = fmt.toLower();
    const QString path = QFileDialog::getSaveFileName(this, "导出测试报告", QString("serialbox-report.%1").arg(ext),
                                                      "文本文件 (*.txt);;JSON 文件 (*.json);;CSV 文件 (*.csv)");
    if (path.isEmpty())
    {
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "测试报告", "无法写入报告文件。");
        return;
    }

    const auto st = m_serialManager->stats();
    const QString title = m_titleEdit ? m_titleEdit->text().trimmed() : QString("SerialBox 测试报告");

    if (fmt == "JSON")
    {
        QJsonObject obj;
        obj.insert("title", title);
        if (m_includeTimestamp->isChecked())
            obj.insert("timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));
        if (m_includePort->isChecked())
            obj.insert("port", m_serialManager->currentPortName());
        if (m_includeTraffic->isChecked())
        {
            obj.insert("rxBytes", static_cast<qint64>(st.rxBytes));
            obj.insert("txBytes", static_cast<qint64>(st.txBytes));
        }
        if (m_includeUptime->isChecked())
            obj.insert("connectedSeconds", st.connectedMs / 1000);
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
    else if (fmt == "CSV")
    {
        QTextStream out(&f);
        out << "field,value\n";
        out << "title," << title << "\n";
        if (m_includeTimestamp->isChecked())
            out << "timestamp," << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
        if (m_includePort->isChecked())
            out << "port," << m_serialManager->currentPortName() << "\n";
        if (m_includeTraffic->isChecked())
        {
            out << "rxBytes," << st.rxBytes << "\n";
            out << "txBytes," << st.txBytes << "\n";
        }
        if (m_includeUptime->isChecked())
            out << "connectedSeconds," << (st.connectedMs / 1000) << "\n";
    }
    else
    {
        QTextStream out(&f);
        out << title << "\n";
        if (m_includeTimestamp->isChecked())
            out << "时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
        if (m_includePort->isChecked())
            out << "端口: " << m_serialManager->currentPortName() << "\n";
        if (m_includeTraffic->isChecked())
        {
            out << "RX: " << st.rxBytes << " B\n";
            out << "TX: " << st.txBytes << " B\n";
        }
        if (m_includeUptime->isChecked())
            out << "连接时长: " << (st.connectedMs / 1000) << " s\n";
    }

    QMessageBox::information(this, "测试报告", "报告导出成功。");
}
