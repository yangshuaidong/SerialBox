#include "SerialBox/protocol/ModbusParser.h"
#include <QVariantList>

ModbusParser::ModbusParser(QObject *parent)
    : IProtocolParser(parent)
{
}

void ModbusParser::notifySent(quint8 fc)
{
    m_lastSentFc = fc;
}

int ModbusParser::expectedFrameLength(const QByteArray &buffer) const
{
    if (buffer.size() < 2) return -1;

    quint8 fc = static_cast<quint8>(buffer[1]);

    // 异常响应: [ID][FC|0x80][ExceptionCode][CRC] = 5 字节
    if (isExceptionResponse(fc)) {
        return 5;
    }

    switch (fc) {
    // ── 读线圈/离散输入/保持寄存器/输入寄存器 ──
    case 0x01: case 0x02: case 0x03: case 0x04: {
        // 关键：通过 m_lastSentFc 判断当前 buffer 是请求还是响应
        // 如果刚发送了相同 FC 的请求，则 buffer 是响应帧
        bool isResponse = (m_lastSentFc == fc);
        if (isResponse && buffer.size() >= 3) {
            int byteCount = static_cast<quint8>(buffer[2]);
            if (byteCount >= 1 && byteCount <= 252) {
                return 3 + byteCount + 2;  // 响应: [ID][FC][ByteCount][Data(N)][CRC]
            }
        }
        // 请求帧或无法确定: 固定 8 字节
        // [ID][FC][AddrH][AddrL][QtyH][QtyL][CRC]
        return 8;
    }

    // ── 写单线圈/单寄存器 ──
    case 0x05: case 0x06:
        return 8;  // 请求/响应格式相同

    // ── 写多线圈 ──
    case 0x0F: {
        // 响应帧固定 8 字节: [ID][FC][AddrH][AddrL][QtyH][QtyL][CRC]
        bool isResponse = (m_lastSentFc == fc);
        if (isResponse) return 8;

        // 请求帧: [ID][FC][AddrH][AddrL][QtyH][QtyL][ByteCount][Data(N)][CRC]
        if (buffer.size() >= 7) {
            int byteCount = static_cast<quint8>(buffer[6]);
            return 7 + byteCount + 2;
        }
        return 8;
    }

    // ── 写多寄存器 ──
    case 0x10: {
        bool isResponse = (m_lastSentFc == fc);
        if (isResponse) return 8;  // 响应固定 8 字节

        // 请求: [ID][FC][AddrH][AddrL][QtyH][QtyL][ByteCount][Data(N)][CRC]
        if (buffer.size() >= 7) {
            int byteCount = static_cast<quint8>(buffer[6]);
            return 7 + byteCount + 2;
        }
        return 8;
    }

    default:
        return -1;  // 未知功能码
    }
}

bool ModbusParser::match(const QByteArray &buffer)
{
    if (buffer.size() < 5) return false;

    int expectedLen = expectedFrameLength(buffer);
    if (expectedLen < 0) return false;
    if (buffer.size() < expectedLen) return false;

    // ★ CRC 校验
    quint16 receivedCrc = static_cast<quint8>(buffer[expectedLen - 2])
                        | (static_cast<quint8>(buffer[expectedLen - 1]) << 8);
    quint16 calcCrc = crc16(buffer.left(expectedLen - 2));
    return receivedCrc == calcCrc;
}

IProtocolParser::ParseResult ModbusParser::parse(QByteArray &buffer)
{
    ParseResult result;
    result.protocolName = name();

    if (!match(buffer)) return result;

    int frameLen = expectedFrameLength(buffer);
    if (frameLen < 0 || buffer.size() < frameLen) return result;

    quint8 slaveId  = static_cast<quint8>(buffer[0]);
    quint8 funcCode = static_cast<quint8>(buffer[1]);

    // CRC（match() 已校验过，此处直接取值用于显示）
    quint16 receivedCrc = static_cast<quint8>(buffer[frameLen - 2])
                        | (static_cast<quint8>(buffer[frameLen - 1]) << 8);
    quint16 calcCrc = crc16(buffer.left(frameLen - 2));

    result.fields["slaveId"]  = slaveId;
    result.fields["funcCode"] = funcCode;
    result.fields["funcName"] = funcCodeName(funcCode);
    result.fields["crcOk"] = (receivedCrc == calcCrc);
    result.fields["crcExpected"] = QString("0x%1").arg(calcCrc, 4, 16, QChar('0')).toUpper();
    result.fields["crcReceived"] = QString("0x%1").arg(receivedCrc, 4, 16, QChar('0')).toUpper();

    // 异常响应
    if (isExceptionResponse(funcCode)) {
        quint8 exCode = static_cast<quint8>(buffer[2]);
        result.fields["exceptionCode"] = exCode;
        result.displayText = QString("[%1] 异常响应 FC=0x%2 ExCode=0x%3")
            .arg(slaveId)
            .arg(funcCode & 0x7F, 2, 16, QChar('0'))
            .arg(exCode, 2, 16, QChar('0'));
        result.matched = true;
        result.rawFrames.append(buffer.left(frameLen));
        buffer.remove(0, frameLen);
        m_lastSentFc = 0;  // 重置
        return result;
    }

    bool isResponse = (m_lastSentFc == funcCode);
    result.fields["direction"] = isResponse ? "response" : "request";

    switch (funcCode) {
    // ── 读线圈/离散输入/保持寄存器/输入寄存器 ──
    case 0x01: case 0x02: case 0x03: case 0x04: {
        if (isResponse) {
            // 响应帧: [ID][FC][ByteCount][Data(N)][CRC]
            int byteCount = static_cast<quint8>(buffer[2]);
            QByteArray data = buffer.mid(3, byteCount);

            result.fields["byteCount"] = byteCount;

            QVariantList registers;
            for (int i = 0; i + 1 < byteCount; i += 2) {
                quint16 val = (static_cast<quint8>(data[i]) << 8)
                            | static_cast<quint8>(data[i + 1]);
                registers.append(val);
            }
            result.fields["registers"] = registers;

            QStringList vals;
            for (auto &v : registers) vals.append(QString::number(v.toUInt()));
            result.displayText = QString("[%1] %2 ×%3  值: %4  CRC: %5")
                .arg(slaveId)
                .arg(funcCodeName(funcCode))
                .arg(byteCount / 2)
                .arg(vals.join(", "))
                .arg(receivedCrc == calcCrc ? "✓" : "✗");
        } else {
            // 请求帧: [ID][FC][AddrH][AddrL][QtyH][QtyL][CRC]
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.displayText = QString("[%1] %2 请求 @%3 ×%4")
                .arg(slaveId)
                .arg(funcCodeName(funcCode))
                .arg(addr)
                .arg(qty);
        }
        break;
    }

    // ── 写单线圈 ──
    case 0x05: {
        quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
        quint16 val  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
        result.fields["address"] = addr;
        result.fields["value"] = val;
        result.fields["coilState"] = (val == 0xFF00) ? "ON" : "OFF";
        result.displayText = QString("[%1] 写单线圈 @%2 = %3")
            .arg(slaveId).arg(addr).arg(val == 0xFF00 ? "ON" : "OFF");
        break;
    }

    // ── 写单寄存器 ──
    case 0x06: {
        quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
        quint16 val  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
        result.fields["address"] = addr;
        result.fields["value"] = val;
        result.displayText = QString("[%1] 写单寄存器 @%2 = %3")
            .arg(slaveId).arg(addr).arg(val);
        break;
    }

    // ── 写多线圈 ──
    case 0x0F: {
        if (isResponse) {
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.displayText = QString("[%1] 写多线圈响应 @%2 ×%3")
                .arg(slaveId).arg(addr).arg(qty);
        } else {
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            int byteCount = static_cast<quint8>(buffer[6]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.fields["byteCount"] = byteCount;
            result.displayText = QString("[%1] 写多线圈 @%2 ×%3 (%4B)")
                .arg(slaveId).arg(addr).arg(qty).arg(byteCount);
        }
        break;
    }

    // ── 写多寄存器 ──
    case 0x10: {
        if (isResponse) {
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.displayText = QString("[%1] 写多寄存器响应 @%2 ×%3")
                .arg(slaveId).arg(addr).arg(qty);
        } else {
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            int byteCount = static_cast<quint8>(buffer[6]);
            QByteArray data = buffer.mid(7, byteCount);

            QVariantList registers;
            for (int i = 0; i + 1 < byteCount; i += 2) {
                quint16 val = (static_cast<quint8>(data[i]) << 8)
                            | static_cast<quint8>(data[i + 1]);
                registers.append(val);
            }

            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.fields["byteCount"] = byteCount;
            result.fields["registers"] = registers;

            QStringList vals;
            for (auto &v : registers) vals.append(QString::number(v.toUInt()));
            result.displayText = QString("[%1] 写多寄存器 @%2 ×%3 值: %4")
                .arg(slaveId).arg(addr).arg(qty).arg(vals.join(", "));
        }
        break;
    }

    default:
        // 未知功能码 — 不消费 buffer，不标记 matched
        result.displayText = QString("[%1] 未知功能码 0x%2")
            .arg(slaveId)
            .arg(funcCode, 2, 16, QChar('0'));
        return result;  // ← 直接返回
    }

    result.matched = true;
    result.rawFrames.append(buffer.left(frameLen));
    buffer.remove(0, frameLen);
    m_lastSentFc = 0;  // 消费后重置
    return result;
}

QByteArray ModbusParser::build(const QVariantMap &fields)
{
    quint8 slaveId  = fields.value("slaveId", 1).toInt();
    quint8 funcCode = fields.value("funcCode", 0x03).toInt();
    quint16 addr    = fields.value("address", 0).toUInt();
    quint16 count   = fields.value("count", fields.value("quantity", 1)).toUInt();

    QByteArray frame;
    frame.append(static_cast<char>(slaveId));
    frame.append(static_cast<char>(funcCode));

    switch (funcCode) {
    case 0x01: case 0x02: case 0x03: case 0x04:
        frame.append(static_cast<char>(addr >> 8));
        frame.append(static_cast<char>(addr & 0xFF));
        frame.append(static_cast<char>(count >> 8));
        frame.append(static_cast<char>(count & 0xFF));
        break;

    case 0x05: {
        quint16 val = fields.value("value", 0xFF00).toUInt();
        frame.append(static_cast<char>(addr >> 8));
        frame.append(static_cast<char>(addr & 0xFF));
        frame.append(static_cast<char>(val >> 8));
        frame.append(static_cast<char>(val & 0xFF));
        break;
    }

    case 0x06: {
        quint16 val = fields.value("value", 0).toUInt();
        frame.append(static_cast<char>(addr >> 8));
        frame.append(static_cast<char>(addr & 0xFF));
        frame.append(static_cast<char>(val >> 8));
        frame.append(static_cast<char>(val & 0xFF));
        break;
    }

    case 0x0F: case 0x10: {
        QVariantList regs = fields.value("registers").toList();
        int byteCount = 0;
        QByteArray data;

        if (funcCode == 0x10) {
            for (auto &v : regs) {
                quint16 val = v.toUInt();
                data.append(static_cast<char>(val >> 8));
                data.append(static_cast<char>(val & 0xFF));
            }
            byteCount = data.size();
            count = regs.size();
        } else {
            data = fields.value("data").toByteArray();
            byteCount = data.size();
        }

        frame.append(static_cast<char>(addr >> 8));
        frame.append(static_cast<char>(addr & 0xFF));
        frame.append(static_cast<char>(count >> 8));
        frame.append(static_cast<char>(count & 0xFF));
        frame.append(static_cast<char>(byteCount));
        frame.append(data);
        break;
    }

    default:
        frame.append(fields.value("data").toByteArray());
    }

    quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>(crc >> 8));

    // 通知解析器：刚发送了此 FC
    // 注意：这里不能直接调用 notifySent，因为 build() 不一定被同一个 parser 调用
    // 调用方应在 sendData 后调用 parser->notifySent(fc)

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
    default:
        if (code & 0x80) return QString("异常响应(FC:%1)").arg(code & 0x7F, 2, 16, QChar('0'));
        return QString("FC:%1").arg(code, 2, 16, QChar('0'));
    }
}
