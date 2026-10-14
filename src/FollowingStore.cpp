#include "FollowingStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

FollowingStore::FollowingStore(QObject* parent)
    : QObject(parent) {
}

QString FollowingStore::filePath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/data/following.json");
}

void FollowingStore::load() {
    m_users.clear();
    m_ownHandle.clear();
    m_updatedAt = QDateTime();

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
    m_ownHandle = obj.value(QStringLiteral("ownHandle")).toString();
    m_updatedAt = QDateTime::fromString(obj.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
    const QJsonArray arr = obj.value(QStringLiteral("users")).toArray();
    for (const auto& v : arr) {
        const QJsonObject u = v.toObject();
        FollowingUser item;
        item.handle = u.value(QStringLiteral("handle")).toString();
        item.name = u.value(QStringLiteral("name")).toString();
        if (!item.handle.isEmpty()) {
            m_users.append(item);
        }
    }
}

bool FollowingStore::save(const QVector<FollowingUser>& users, const QString& ownHandle) {
    QDir().mkpath(QCoreApplication::applicationDirPath() + QStringLiteral("/data"));
    m_users = users;
    m_ownHandle = ownHandle;
    m_updatedAt = QDateTime::currentDateTime();

    QJsonObject obj;
    obj.insert(QStringLiteral("ownHandle"), m_ownHandle);
    obj.insert(QStringLiteral("updatedAt"), m_updatedAt.toString(Qt::ISODate));
    QJsonArray arr;
    for (const auto& u : m_users) {
        QJsonObject item;
        item.insert(QStringLiteral("handle"), u.handle);
        item.insert(QStringLiteral("name"), u.name);
        arr.append(item);
    }
    obj.insert(QStringLiteral("users"), arr);

    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}
