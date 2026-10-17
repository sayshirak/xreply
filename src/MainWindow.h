#ifndef XREPLY_MAINWINDOW_H
#define XREPLY_MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QVector>

class AutoReplier;
class BrowserWidget;
class FollowingStore;
class ReplyStore;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QTimer;
class QTimerEvent;

struct ReplyTarget {
    QString handle;
    QString name;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void timerEvent(QTimerEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onBrowserCreated();
    void onLoadFinished(bool success);
    void onUrlChanged(const QString& url);
    void onJsResult(const QString& result);
    void startBrowserIfNeeded();
    void onStartClicked();
    void onUpdateFollowingClicked();
    void onPauseClicked();
    void processQueue();
    void appendLog(const QString& message);

private:
    enum class Phase {
        Idle,
        Exporting,
        Cooling,
        OpeningProfile,
        Scanning,
        OpeningPost,
        Replying
    };

    void setupUi();
    void loadSettings();
    void saveSettings();
    QStringList effectiveKeywords() const;
    QString effectiveReplyText() const;
    void refreshFollowingStatus();
    void setControlsForIdle();
    void setControlsForExporting();
    void setControlsForReplying();
    void beginExport(bool startReplyAfter);
    void beginReplyFromCache();
    void finishExportAndSave();
    void goNextAfter(int delayMs);
    void openNextProfile();
    void updateStatusBar();
    void rememberScannedUser(const QString& handle);
    void applyResumeIndex();
    void finishQueueExhausted();

    BrowserWidget* m_browser = nullptr;
    AutoReplier* m_replier = nullptr;
    ReplyStore* m_replyStore = nullptr;
    FollowingStore* m_followingStore = nullptr;

    QLabel* m_statusLabel = nullptr;
    QTextEdit* m_logTextEdit = nullptr;
    QLineEdit* m_keywordsEdit = nullptr;
    QLineEdit* m_replyEdit = nullptr;
    QSpinBox* m_cooldownSpin = nullptr;
    QSpinBox* m_dailyLimitSpin = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_updateFollowingBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QLabel* m_followingLabel = nullptr;
    QLabel* m_statsLabel = nullptr;

    QTimer* m_queueTimer = nullptr;
    int m_cefTimerId = 0;
    bool m_browserStartRequested = false;
    bool m_running = false;      // 互评中
    bool m_exporting = false;    // 导出/更新列表中
    bool m_startReplyAfterExport = false;
    bool m_waitingLoad = false;
    Phase m_phase = Phase::Idle;

    QString m_ownHandle;
    QVector<ReplyTarget> m_queue;
    int m_index = 0;
    QString m_lastScannedHandle; // 上次扫描结果对应的用户，续扫从其下一位开始
    QString m_currentHandle;
    QString m_currentPostId;
    QString m_currentPostUrl;
    int m_sessionReplied = 0;
};

#endif
