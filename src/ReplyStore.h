#ifndef XREPLY_REPLYSTORE_H
#define XREPLY_REPLYSTORE_H

#include <QDate>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

// 每日回复去重：每用户每天最多一次；跨日自动清空当日集合
class ReplyStore : public QObject {
    Q_OBJECT
public:
    explicit ReplyStore(QObject* parent = nullptr);

    void load();
    void save() const;

    QDate today() const { return m_date; }
    int todayCount() const { return m_handles.size(); }

    bool hasRepliedToday(const QString& handle) const;
    void markReplied(const QString& handle, const QString& postId);

    // 若日期变化则清空并返回 true
    bool rollDayIfNeeded();

private:
    QString filePath() const;

    QDate m_date;
    QSet<QString> m_handles;
    QHash<QString, QString> m_postIds; // handle -> last postId
};

#endif
