#pragma once
#include "IProtocolParser.h"
#include <QJsonDocument>

/**
 * JsonParser — JSON 帧解析
 *
 * 以换行符或 } 分隔，自动提取 JSON 对象
 */
class JsonParser : public IProtocolParser
{
    Q_OBJECT

public:
    explicit JsonParser(QObject *parent = nullptr);

    QString name() const override { return "JSON"; }
    QString description() const override { return "JSON 帧解析与格式化"; }

    bool match(const QByteArray &buffer) override;
    ParseResult parse(QByteArray &buffer) override;
    QByteArray build(const QVariantMap &fields) override;

    static QString prettyPrint(const QJsonDocument &doc);
};
