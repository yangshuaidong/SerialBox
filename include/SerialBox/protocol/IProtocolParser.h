/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/protocol/IProtocolParser.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QByteArray>
#include <QVariantMap>

/**
 * IProtocolParser — 通用协议解析器接口
 *
 * 标准化 match / parse / build 流程
 */
class IProtocolParser : public QObject
{
    Q_OBJECT

public:
    explicit IProtocolParser(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IProtocolParser() = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;

    // ── 数据 → 结构化报文 ──
    struct ParseResult {
        bool matched = false;
        QString protocolName;
        QVariantMap fields;          // 结构化字段
        QString displayText;         // 人类可读文本
        QList<QByteArray> rawFrames; // 完整帧
    };

    /**
     * match: 缓冲区中是否包含完整帧
     * parse: 提取并解析帧内容
     * build: 从字段构建发送帧
     */
    virtual bool match(const QByteArray &buffer) = 0;
    virtual ParseResult parse(QByteArray &buffer) = 0;
    virtual QByteArray build(const QVariantMap &fields) = 0;
};
