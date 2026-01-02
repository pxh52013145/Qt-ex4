#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace Protocol {

constexpr quint16 kDefaultPort = 45454;
constexpr int kMaxNameLength = 20;
constexpr int kMaxMessageLength = 500;

inline QByteArray toLine(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

inline QString normalizeName(QString name)
{
    return name.trimmed();
}

inline QString normalizeText(QString text)
{
    return text.trimmed();
}

inline bool isValidName(const QString &name)
{
    return !name.isEmpty() && name.size() <= kMaxNameLength;
}

inline bool isValidMessage(const QString &text)
{
    return !text.isEmpty() && text.size() <= kMaxMessageLength;
}

} // namespace Protocol

