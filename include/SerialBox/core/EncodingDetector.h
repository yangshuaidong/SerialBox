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
