#include "SerialBox/protocol/ModbusParser.h"
#include <QVariantList>

ModbusParser::ModbusParser(QObject *parent)
    : IProtocolParser(parent)
{
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
    case 0x01: case 0x02: case 0x03: case 0x04:
        // 请求帧: [ID][FC][AddrH][AddrL][QtyH][QtyL][CRC] = 8
        // 响应帧: [ID][FC][ByteCount][Data(N)][CRC] = 3+N+2
        // 通过第三个字节判断：如果 buffer[2] * 2 + 5 == buffer.size() 则为响应
        // 更可靠：请求固定 8 字节；响应第三个字节是 ByteCount
        if (buffer.size() >= 3) {
            int byteCount = static_cast<quint8>(buffer[2]);
            int responseLen = 3 + byteCount + 2;
            // 如果 buffer 长度 >= responseLen 且 byteCount 合理 (1-252)
            if (byteCount >= 1 && byteCount <= 252 && buffer.size() >= responseLen) {
                return responseLen;
            }
        }
        // 默认按请求帧处理
        return 8;

    // ── 写单线圈/单寄存器 ──
    case 0x05: case 0x06:
        // 请求/响应格式相同: [ID][FC][AddrH][AddrL][ValH][ValL][CRC] = 8
        return 8;

    // ── 写多线圈 ──
    case 0x0F: {
        // 请求: [ID][FC][AddrH][AddrL][QtyH][QtyL][ByteCount][Data(N)][CRC]
        // 响应: [ID][FC][AddrH][AddrL][QtyH][QtyL][CRC] = 8
        if (buffer.size() >= 7) {
            int byteCount = static_cast<quint8>(buffer[6]);
            int requestLen = 7 + byteCount + 2;
            if (buffer.size() >= requestLen) {
                return requestLen;
            }
        }
        // 响应帧 8 字节
        return 8;
    }

    // ── 写多寄存器 ──
    case 0x10: {
        // 请求: [ID][FC][AddrH][AddrL][QtyH][QtyL][ByteCount][Data(N)][CRC]
        // 响应: [ID][FC][AddrH][AddrL][QtyH][QtyL][CRC] = 8
        if (buffer.size() >= 7) {
            int byteCount = static_cast<quint8>(buffer[6]);
            int requestLen = 7 + byteCount + 2;
            if (buffer.size() >= requestLen) {
                return requestLen;
            }
        }
        return 8;
    }

    default:
        return -1; // 未知功能码
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

    // CRC 验证结果
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
        return result;
    }

    switch (funcCode) {
    // ── 读保持/输入寄存器 响应 ──
    case 0x03: case 0x04: {
        if (buffer.size() >= 3 && frameLen > 8) {
            // 响应帧
            int byteCount = static_cast<quint8>(buffer[2]);
            QByteArray data = buffer.mid(3, byteCount);

            result.fields["byteCount"] = byteCount;
            result.fields["direction"] = "response";

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
            // 请求帧
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.fields["direction"] = "request";
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
        if (frameLen == 8) {
            // 响应帧
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.fields["direction"] = "response";
            result.displayText = QString("[%1] 写多线圈响应 @%2 ×%3")
                .arg(slaveId).arg(addr).arg(qty);
        } else {
            // 请求帧
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            int byteCount = static_cast<quint8>(buffer[6]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.fields["byteCount"] = byteCount;
            result.fields["direction"] = "request";
            result.displayText = QString("[%1] 写多线圈 @%2 ×%3 (%4B)")
                .arg(slaveId).arg(addr).arg(qty).arg(byteCount);
        }
        break;
    }

    // ── 写多寄存器 ──
    case 0x10: {
        if (frameLen == 8) {
            // 响应帧
            quint16 addr = (static_cast<quint8>(buffer[2]) << 8) | static_cast<quint8>(buffer[3]);
            quint16 qty  = (static_cast<quint8>(buffer[4]) << 8) | static_cast<quint8>(buffer[5]);
            result.fields["address"] = addr;
            result.fields["quantity"] = qty;
            result.fields["direction"] = "response";
            result.displayText = QString("[%1] 写多寄存器响应 @%2 ×%3")
                .arg(slaveId).arg(addr).arg(qty);
        } else {
            // 请求帧
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
            result.fields["direction"] = "request";

            QStringList vals;
            for (auto &v : registers) vals.append(QString::number(v.toUInt()));
            result.displayText = QString("[%1] 写多寄存器 @%2 ×%3 值: %4")
                .arg(slaveId).arg(addr).arg(qty).arg(vals.join(", "));
        }
        break;
    }

    default:
        result.displayText = QString("[%1] 未知功能码 0x%2")
            .arg(slaveId)
            .arg(funcCode, 2, 16, QChar('0'));
    }

    result.matched = true;
    result.rawFrames.append(buffer.left(frameLen));
    buffer.remove(0, frameLen);
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
        // 读请求
        frame.append(static_cast<char>(addr >> 8));
        frame.append(static_cast<char>(addr & 0xFF));
        frame.append(static_cast<char>(count >> 8));
        frame.append(static_cast<char>(count & 0xFF));
        break;

    case 0x05: {
        // 写单线圈
        quint16 val = fields.value("value", 0xFF00).toUInt();
        frame.append(static_cast<char>(addr >> 8));
        frame.append(static_cast<char>(addr & 0xFF));
        frame.append(static_cast<char>(val >> 8));
        frame.append(static_cast<char>(val & 0xFF));
        break;
    }

    case 0x06: {
        // 写单寄存器
        quint16 val = fields.value("value", 0).toUInt();
        frame.append(static_cast<char>(addr >> 8));
        frame.append(static_cast<char>(addr & 0xFF));
        frame.append(static_cast<char>(val >> 8));
        frame.append(static_cast<char>(val & 0xFF));
        break;
    }

    case 0x0F: case 0x10: {
        // 写多线圈/寄存器
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
            // 0x0F: 线圈，每 bit 一个
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
        // 未知功能码，原样拼接原始 data
        frame.append(fields.value("data").toByteArray());
    }

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
    default:
        if (code & 0x80) return QString("异常响应(FC:%1)").arg(code & 0x7F, 2, 16, QChar('0'));
        return QString("FC:%1").arg(code, 2, 16, QChar('0'));
    }
}
