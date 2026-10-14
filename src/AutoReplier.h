#ifndef XREPLY_AUTOREPLIER_H
#define XREPLY_AUTOREPLIER_H

#include <QObject>
#include <QString>
#include <QStringList>

// 互评 JS 脚本工厂（仅供 xreply 使用）
class AutoReplier : public QObject {
    Q_OBJECT
public:
    explicit AutoReplier(QObject* parent = nullptr);

    // 导出正在关注中的蓝V用户
    QString getExportBlueFollowingScript() const;

    // 在用户主页检测第一条本人原创帖：须为本地今日发布且含关键词
    QString getScanLatestPostScript(const QStringList& keywords) const;

    // 在帖子页回复指定文案
    QString getReplyScript(const QString& replyText) const;
};

#endif
