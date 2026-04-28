/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/core/EncodingDetector.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/core/EncodingDetector.h"
#include <QStringDecoder>

EncodingDetector::Encoding EncodingDetector::detect(const QByteArray &data)
{
    if (data.isEmpty()) return Encoding::ASCII;

    // 1. 检查是否为合法 UTF-8
    if (isValidUtf8(data)) {
        bool hasMultiByte = false;
        for (int i = 0; i < data.size(); ++i) {
            auto c = static_cast<unsigned char>(data[i]);
            if (c > 0x7F) { hasMultiByte = true; break; }
        }
        return hasMultiByte ? Encoding::UTF8 : Encoding::ASCII;
    }

    // 2. 尝试 GBK / GB18030 — 用 QByteArray 转换探测
    //    QTextCodec 在 Qt6 中移至 Core5Compat，这里用 QStringDecoder(Latin1) 做 fallback
    //    对于 GBK 数据，Latin1 解码不会报错（0x00-0xFF 全部合法）
    //    但可以靠"非 ASCII 且非合法 UTF-8"来推断可能是 GBK
    //    更可靠的方案：安装 Qt6Core5Compat 使用 QTextCodec
    //    这里做简化处理：非 UTF-8 的非 ASCII 数据 → 视为 GBK
    return Encoding::GBK;
}

bool EncodingDetector::isValidUtf8(const QByteArray &data)
{
    int i = 0;
    const int len = data.size();
    const auto *d = reinterpret_cast<const unsigned char *>(data.constData());

    while (i < len) {
        int bytes;
        if (d[i] <= 0x7F)              bytes = 1;
        else if ((d[i] & 0xE0) == 0xC0) bytes = 2;
        else if ((d[i] & 0xF0) == 0xE0) bytes = 3;
        else if ((d[i] & 0xF8) == 0xF0) bytes = 4;
        else return false;

        if (i + bytes > len) return false;
        for (int j = 1; j < bytes; ++j) {
            if ((d[i + j] & 0xC0) != 0x80) return false;
        }
        i += bytes;
    }
    return true;
}

QString EncodingDetector::decode(const QByteArray &data, Encoding encoding)
{
    switch (encoding) {
    case Encoding::UTF8:
        return QString::fromUtf8(data);
    case Encoding::GBK: {
        // Qt6 原生 QStringDecoder 不支持 GBK 枚举
        // 使用 fromLatin1 作为降级（GBK 字节不会丢失，显示可能不完美）
        // 生产环境建议加 Qt6Core5Compat
        return QString::fromLatin1(data);
    }
    case Encoding::ASCII:
    case Encoding::Unknown:
    default:
        return QString::fromLatin1(data);
    }
}

QByteArray EncodingDetector::encode(const QString &text, Encoding encoding)
{
    switch (encoding) {
    case Encoding::UTF8:
        return text.toUtf8();
    case Encoding::GBK:
        return text.toLatin1();
    case Encoding::ASCII:
    case Encoding::Unknown:
    default:
        return text.toLatin1();
    }
}

QString EncodingDetector::detectAndDecode(const QByteArray &data)
{
    return decode(data, detect(data));
}
