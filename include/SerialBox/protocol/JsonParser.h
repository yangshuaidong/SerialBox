/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/protocol/JsonParser.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
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
