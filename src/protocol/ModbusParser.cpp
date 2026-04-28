/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/protocol/ModbusParser.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/protocol/ModbusParser.h"
#include <QVariantList>

// Modbus RTU 帧格式: [SlaveID(1)] [FuncCode(1)] [Data(N)] [CRC16(2)]

ModbusParser::ModbusParser(QObject *parent)
    : IProtocolParser(parent)
{
}

bool ModbusParser::match(const QByteArray &buffer)
{
    if (buffer.size() < 5) return false; // 最短帧: ID(1) + FC(1) + Data(1) + CRC(2)

    quint8 funcCode = static_cast<quint8>(buffer[1]);
    int expectedLen = -1;

    switch (funcCode) {
    case 0x03: case 0x04: // 读保持/输入寄存器
        if (buffer.size() >= 3) {
            int byteCount = static_cast<quint8>(buffer[2]);
            expectedLen = 3 + byteCount + 2; // header + data + crc
        }
        break;
    case 0x06: // 写单寄存器
        expectedLen = 8; // ID(1) + FC(1) + Addr(2) + Val(2) + CRC(2)
        break;
    case 0x10: // 写多寄存器
        expectedLen = 8;
        break;
    default:
        // 未知功能码，至少 5 字节
        expectedLen = 5;
    }

    return buffer.size() >= expectedLen;
}

IProtocolParser::ParseResult ModbusParser::parse(QByteArray &buffer)
{
    ParseResult result;
    result.protocolName = name();

    if (!match(buffer)) return result;

    quint8 slaveId   = static_cast<quint8>(buffer[0]);
    quint8 funcCode  = static_cast<quint8>(buffer[1]);
    int frameLen = 0;

    result.fields["slaveId"]  = slaveId;
    result.fields["funcCode"] = funcCode;
    result.fields["funcName"] = funcCodeName(funcCode);

    switch (funcCode) {
    case 0x03: case 0x04: {
        int byteCount = static_cast<quint8>(buffer[2]);
        frameLen = 3 + byteCount + 2;
        if (buffer.size() < frameLen) return result;

        QByteArray data = buffer.mid(3, byteCount);
        // CRC 校验
        quint16 receivedCrc = static_cast<quint8>(buffer[frameLen - 2])
                            | (static_cast<quint8>(buffer[frameLen - 1]) << 8);
        quint16 calcCrc = crc16(buffer.left(frameLen - 2));

        result.fields["byteCount"] = byteCount;
        result.fields["crcOk"] = (receivedCrc == calcCrc);
        result.fields["crcExpected"] = QString("0x%1").arg(calcCrc, 4, 16, QChar('0').toUpper());
        result.fields["crcReceived"] = QString("0x%1").arg(receivedCrc, 4, 16, QChar('0').toUpper());

        // 寄存器值
        QVariantList registers;
        for (int i = 0; i < byteCount; i += 2) {
            quint16 val = (static_cast<quint8>(data[i]) << 8)
                        | static_cast<quint8>(data[i + 1]);
            registers.append(val);
        }
        result.fields["registers"] = registers;

        result.displayText = QString("[%1] %2 ×%3  值: %4  CRC: %5")
            .arg(slaveId)
            .arg(funcCodeName(funcCode))
            .arg(byteCount / 2)
            .arg([&]() {
                QStringList vals;
                for (auto &v : registers) vals.append(QString::number(v.toUInt()));
                return vals.join(", ");
            }())
            .arg(receivedCrc == calcCrc ? "✓" : "✗");
        break;
    }
    case 0x06: {
        frameLen = 8;
        if (buffer.size() < frameLen) return result;
        quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
        quint16 val  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
        result.fields["address"] = addr;
        result.fields["value"]   = val;
        result.displayText = QString("[%1] 写单寄存器 @%2 = %3")
            .arg(slaveId).arg(addr).arg(val);
        break;
    }
    default:
        frameLen = 5;
        result.displayText = QString("[%1] 未知功能码 0x%2")
            .arg(slaveId)
            .arg(funcCode, 2, 16, QChar('0'));
    }

    if (frameLen > 0) {
        result.matched = true;
        result.rawFrames.append(buffer.left(frameLen));
        buffer.remove(0, frameLen);
    }

    return result;
}

QByteArray ModbusParser::build(const QVariantMap &fields)
{
    quint8 slaveId  = fields.value("slaveId", 1).toInt();
    quint8 funcCode = fields.value("funcCode", 0x03).toInt();
    quint16 addr    = fields.value("address", 0).toUInt();
    quint16 count   = fields.value("count", 1).toUInt();

    QByteArray frame;
    frame.append(static_cast<char>(slaveId));
    frame.append(static_cast<char>(funcCode));
    frame.append(static_cast<char>(addr >> 8));
    frame.append(static_cast<char>(addr & 0xFF));
    frame.append(static_cast<char>(count >> 8));
    frame.append(static_cast<char>(count & 0xFF));

    quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>(crc >> 8));

    return frame;
}

quint16 ModbusParser::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (auto byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) { crc >>= 1; crc ^= 0xA001; }
            else { crc >>= 1; }
        }
    }
    return crc;
}

QString ModbusParser::funcCodeName(quint8 code)
{
    switch (code) {
    case 0x01: return "读线圈";
    case 0x02: return "读离散输入";
    case 0x03: return "读保持寄存器";
    case 0x04: return "读输入寄存器";
    case 0x05: return "写单线圈";
    case 0x06: return "写单寄存器";
    case 0x0F: return "写多线圈";
    case 0x10: return "写多寄存器";
    default:   return QString("FC:%1").arg(code, 2, 16, QChar('0'));
    }
}
