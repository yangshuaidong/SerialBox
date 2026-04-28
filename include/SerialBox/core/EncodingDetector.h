/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/core/EncodingDetector.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QByteArray>
#include <QString>

/**
 * EncodingDetector — 编码自动检测 (UTF-8 / GBK / ASCII)
 *
 * 智能 fallback 防乱码
 */
class EncodingDetector
{
public:
    enum class Encoding { UTF8, GBK, ASCII, Unknown };

    static Encoding detect(const QByteArray &data);
    static QString decode(const QByteArray &data, Encoding encoding);
    static QByteArray encode(const QString &text, Encoding encoding);

    static QString detectAndDecode(const QByteArray &data);

private:
    static bool isValidUtf8(const QByteArray &data);
};
