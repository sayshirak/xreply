#ifndef XREPLY_FOLLOWINGSTORE_H
#define XREPLY_FOLLOWINGSTORE_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

struct FollowingUser {
    QString handle;
    QString name;
};

// 蓝V关注列表缓存 data/following.json
class FollowingStore : public QObject {
    Q_OBJECT
public:
    explicit FollowingStore(QObject* parent = nullptr);

    void load();
    bool save(const QVector<FollowingUser>& users, const QString& ownHandle);

    bool hasList() const { return !m_users.isEmpty(); }
    int count() const { return m_users.size(); }
    QString ownHandle() const { return m_ownHandle; }
    QDateTime updatedAt() const { return m_updatedAt; }
    QVector<FollowingUser> users() const { return m_users; }

    static QString filePath();

private:
    QVector<FollowingUser> m_users;
    QString m_ownHandle;
    QDateTime m_updatedAt;
};

#endif
