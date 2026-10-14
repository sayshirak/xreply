#include "ReplyStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ReplyStore::ReplyStore(QObject* parent)
    : QObject(parent)
    , m_date(QDate::currentDate()) {
}

QString ReplyStore::filePath() const {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/data/replied.json");
}

void ReplyStore::load() {
    rollDayIfNeeded();
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();
    const QDate d = QDate::fromString(obj.value(QStringLiteral("date")).toString(), Qt::ISODate);
    if (!d.isValid() || d != QDate::currentDate()) {
        m_date = QDate::currentDate();
        m_handles.clear();
        m_postIds.clear();
        return;
    }
    m_date = d;
    m_handles.clear();
    m_postIds.clear();
    const QJsonArray arr = obj.value(QStringLiteral("handles")).toArray();
    for (const auto& v : arr) {
        const QJsonObject item = v.toObject();
        const QString h = item.value(QStringLiteral("handle")).toString().toLower();
        if (h.isEmpty()) {
            continue;
        }
        m_handles.insert(h);
        m_postIds.insert(h, item.value(QStringLiteral("postId")).toString());
    }
}

void ReplyStore::save() const {
    QDir().mkpath(QCoreApplication::applicationDirPath() + QStringLiteral("/data"));
    QJsonObject obj;
    obj.insert(QStringLiteral("date"), m_date.toString(Qt::ISODate));
    QJsonArray arr;
    for (const QString& h : m_handles) {
        QJsonObject item;
        item.insert(QStringLiteral("handle"), h);
        item.insert(QStringLiteral("postId"), m_postIds.value(h));
        arr.append(item);
    }
    obj.insert(QStringLiteral("handles"), arr);
    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

bool ReplyStore::hasRepliedToday(const QString& handle) const {
    return m_handles.contains(handle.toLower());
}

void ReplyStore::markReplied(const QString& handle, const QString& postId) {
    rollDayIfNeeded();
    const QString h = handle.toLower();
    m_handles.insert(h);
    m_postIds.insert(h, postId);
    save();
}

bool ReplyStore::rollDayIfNeeded() {
    const QDate today = QDate::currentDate();
    if (m_date == today) {
        return false;
    }
    m_date = today;
    m_handles.clear();
    m_postIds.clear();
    save();
    return true;
}
