#include "MainWindow.h"
#include "App/CefApp.h"
#include "AutoReplier.h"
#include "FollowingStore.h"
#include "ReplyStore.h"
#include "UI/BrowserWidget.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("X互评助手"));
    resize(1280, 860);

    m_replier = new AutoReplier(this);
    m_replyStore = new ReplyStore(this);
    m_replyStore->load();
    m_followingStore = new FollowingStore(this);
    m_followingStore->load();

    m_queueTimer = new QTimer(this);
    m_queueTimer->setSingleShot(true);
    connect(m_queueTimer, &QTimer::timeout, this, &MainWindow::processQueue);

    setupUi();
    loadSettings();
    refreshFollowingStatus();
    updateStatusBar();
    appendLog(QStringLiteral("登录后可「开始互评」。无缓存名单时会先提示导出；关键词空=「互评」，回复空=「来串门」。"));
    appendLog(QStringLiteral("今日已记录回复 %1/%2 人（每用户每天最多 1 次）")
                  .arg(m_replyStore->todayCount())
                  .arg(m_dailyLimitSpin->value()));

    m_cefTimerId = startTimer(10);
}

MainWindow::~MainWindow() {
    saveSettings();
    if (m_cefTimerId) {
        killTimer(m_cefTimerId);
        m_cefTimerId = 0;
    }
    if (m_browser) {
        m_browser->DisconnectAll();
        m_browser->CloseBrowser();
    }
}

void MainWindow::timerEvent(QTimerEvent* event) {
    if (event->timerId() == m_cefTimerId) {
        CefHelper::DoMessageLoopWork();
    }
    QMainWindow::timerEvent(event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    QTimer::singleShot(0, this, &MainWindow::startBrowserIfNeeded);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::startBrowserIfNeeded() {
    if (m_browserStartRequested || !m_browser) {
        return;
    }
    m_browserStartRequested = true;
    m_statusLabel->setText(QStringLiteral("状态: 正在创建浏览器…"));
    m_browser->CreateBrowser(QStringLiteral("https://x.com/home"));
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);

    m_statusLabel = new QLabel(QStringLiteral("状态: 等待窗口显示…"), central);
    root->addWidget(m_statusLabel);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    root->addWidget(splitter, 1);

    m_browser = new BrowserWidget(splitter);
    m_browser->setMinimumWidth(480);
    m_browser->setMinimumHeight(360);
    splitter->addWidget(m_browser);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(6, 0, 0, 0);

    auto* box = new QGroupBox(QStringLiteral("互评设置"), right);
    auto* form = new QFormLayout(box);
    m_keywordsEdit = new QLineEdit(box);
    m_keywordsEdit->setPlaceholderText(QStringLiteral("空=默认「互评」；多个用逗号/空格分隔"));
    m_replyEdit = new QLineEdit(box);
    m_replyEdit->setPlaceholderText(QStringLiteral("空=默认「来串门」"));
    m_cooldownSpin = new QSpinBox(box);
    m_cooldownSpin->setRange(10, 3600);
    m_cooldownSpin->setValue(90);
    m_cooldownSpin->setSuffix(QStringLiteral(" 秒"));
    m_dailyLimitSpin = new QSpinBox(box);
    m_dailyLimitSpin->setRange(1, 5000);
    m_dailyLimitSpin->setValue(100);
    form->addRow(QStringLiteral("检测关键词"), m_keywordsEdit);
    form->addRow(QStringLiteral("回复文案"), m_replyEdit);
    form->addRow(QStringLiteral("回复间隔"), m_cooldownSpin);
    form->addRow(QStringLiteral("每日上限"), m_dailyLimitSpin);
    rightLayout->addWidget(box);

    auto* btnRow = new QHBoxLayout();
    m_startBtn = new QPushButton(QStringLiteral("开始互评"), right);
    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), right);
    m_pauseBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_pauseBtn);
    rightLayout->addLayout(btnRow);

    m_updateFollowingBtn = new QPushButton(QStringLiteral("更新蓝V关注列表"), right);
    rightLayout->addWidget(m_updateFollowingBtn);

    m_followingLabel = new QLabel(right);
    m_followingLabel->setWordWrap(true);
    rightLayout->addWidget(m_followingLabel);

    m_statsLabel = new QLabel(QStringLiteral("队列: 0/0 | 本次已回: 0 | 今日: 0/0 | 缓存: 0"), right);
    rightLayout->addWidget(m_statsLabel);

    m_logTextEdit = new QTextEdit(right);
    m_logTextEdit->setReadOnly(true);
    rightLayout->addWidget(m_logTextEdit, 1);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    connect(m_browser, &BrowserWidget::browserCreated, this, &MainWindow::onBrowserCreated);
    connect(m_browser, &BrowserWidget::loadFinished, this, &MainWindow::onLoadFinished);
    connect(m_browser, &BrowserWidget::urlChanged, this, &MainWindow::onUrlChanged);
    connect(m_browser, &BrowserWidget::jsResultReceived, this, &MainWindow::onJsResult);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_updateFollowingBtn, &QPushButton::clicked, this, &MainWindow::onUpdateFollowingClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(m_dailyLimitSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        updateStatusBar();
    });
}

void MainWindow::loadSettings() {
    QSettings s(QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini"),
                QSettings::IniFormat);
    m_keywordsEdit->setText(s.value(QStringLiteral("keywords")).toString());
    m_replyEdit->setText(s.value(QStringLiteral("replyText")).toString());
    m_cooldownSpin->setValue(s.value(QStringLiteral("cooldownSec"), 90).toInt());
    m_dailyLimitSpin->setValue(s.value(QStringLiteral("dailyLimit"), 100).toInt());
    m_lastScannedHandle = s.value(QStringLiteral("lastScannedHandle")).toString().trimmed();
}

void MainWindow::saveSettings() {
    QSettings s(QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini"),
                QSettings::IniFormat);
    s.setValue(QStringLiteral("keywords"), m_keywordsEdit->text());
    s.setValue(QStringLiteral("replyText"), m_replyEdit->text());
    s.setValue(QStringLiteral("cooldownSec"), m_cooldownSpin->value());
    s.setValue(QStringLiteral("dailyLimit"), m_dailyLimitSpin->value());
    s.setValue(QStringLiteral("lastScannedHandle"), m_lastScannedHandle);
}

void MainWindow::refreshFollowingStatus() {
    if (!m_followingStore->hasList()) {
        m_followingLabel->setText(QStringLiteral("蓝V列表: 无缓存（冷启动需先导出）"));
        return;
    }
    const QString when = m_followingStore->updatedAt().isValid()
                             ? m_followingStore->updatedAt().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                             : QStringLiteral("未知");
    m_followingLabel->setText(QStringLiteral("蓝V列表: %1 人 | 更新于 %2")
                                  .arg(m_followingStore->count())
                                  .arg(when));
}

QStringList MainWindow::effectiveKeywords() const {
    QStringList raw = m_keywordsEdit->text().split(QRegularExpression(QStringLiteral("[,，\\s]+")),
                                                   Qt::SkipEmptyParts);
    QStringList out;
    for (QString k : raw) {
        k = k.trimmed();
        if (!k.isEmpty()) {
            out.append(k);
        }
    }
    if (out.isEmpty()) {
        out.append(QStringLiteral("互评"));
    }
    return out;
}

QString MainWindow::effectiveReplyText() const {
    const QString t = m_replyEdit->text().trimmed();
    return t.isEmpty() ? QStringLiteral("来串门") : t;
}

void MainWindow::setControlsForIdle() {
    m_startBtn->setEnabled(true);
    m_updateFollowingBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
    m_keywordsEdit->setEnabled(true);
    m_replyEdit->setEnabled(true);
    m_cooldownSpin->setEnabled(true);
    m_dailyLimitSpin->setEnabled(true);
}

void MainWindow::setControlsForExporting() {
    m_startBtn->setEnabled(false);
    m_updateFollowingBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
    m_pauseBtn->setText(QStringLiteral("取消导出"));
    m_keywordsEdit->setEnabled(false);
    m_replyEdit->setEnabled(false);
    m_cooldownSpin->setEnabled(false);
    m_dailyLimitSpin->setEnabled(false);
}

void MainWindow::setControlsForReplying() {
    m_startBtn->setEnabled(false);
    m_updateFollowingBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
    m_pauseBtn->setText(QStringLiteral("暂停"));
    m_keywordsEdit->setEnabled(false);
    m_replyEdit->setEnabled(false);
    m_cooldownSpin->setEnabled(false);
    m_dailyLimitSpin->setEnabled(false);
}

void MainWindow::beginExport(bool startReplyAfter) {
    if (!m_browser) {
        return;
    }
    m_startReplyAfterExport = startReplyAfter;
    m_exporting = true;
    m_running = false;
    m_queue.clear();
    m_index = 0;
    m_sessionReplied = 0;
    m_phase = Phase::Exporting;
    m_waitingLoad = true;
    setControlsForExporting();
    updateStatusBar();
    appendLog(startReplyAfter
                  ? QStringLiteral("开始导出蓝V关注列表（期间不能互评）…")
                  : QStringLiteral("开始更新蓝V关注列表（期间不能互评）…"));
    m_browser->ExecuteJavaScript(m_replier->getExportBlueFollowingScript());
}

void MainWindow::beginReplyFromCache() {
    if (m_replyStore->rollDayIfNeeded()) {
        appendLog(QStringLiteral("已跨日，今日回复列表已刷新"));
        updateStatusBar();
    }
    m_queue.clear();
    for (const auto& u : m_followingStore->users()) {
        ReplyTarget t;
        t.handle = u.handle;
        t.name = u.name;
        m_queue.append(t);
    }
    m_ownHandle = m_followingStore->ownHandle();
    applyResumeIndex();
    m_sessionReplied = 0;
    m_exporting = false;
    m_running = true;
    m_phase = Phase::Cooling;
    setControlsForReplying();
    updateStatusBar();
    appendLog(QStringLiteral("使用缓存名单 %1 人开始互评。关键词=%2 回复=\"%3\" 间隔=%4s 日上限=%5")
                  .arg(m_queue.size())
                  .arg(effectiveKeywords().join(QStringLiteral(",")))
                  .arg(effectiveReplyText())
                  .arg(m_cooldownSpin->value())
                  .arg(m_dailyLimitSpin->value()));
    goNextAfter(500);
}

void MainWindow::finishExportAndSave() {
    QVector<FollowingUser> users;
    users.reserve(m_queue.size());
    for (const auto& t : m_queue) {
        FollowingUser u;
        u.handle = t.handle;
        u.name = t.name;
        users.append(u);
    }
    if (!m_followingStore->save(users, m_ownHandle)) {
        appendLog(QStringLiteral("写入 data/following.json 失败"));
    } else {
        appendLog(QStringLiteral("已写入 data/following.json，共 %1 人").arg(users.size()));
    }
    refreshFollowingStatus();

    const bool thenReply = m_startReplyAfterExport;
    m_exporting = false;
    m_startReplyAfterExport = false;

    if (thenReply && !m_queue.isEmpty()) {
        m_running = true;
        applyResumeIndex();
        m_sessionReplied = 0;
        setControlsForReplying();
        appendLog(QStringLiteral("导出完成，开始互评…"));
        goNextAfter(1500);
    } else {
        m_phase = Phase::Idle;
        setControlsForIdle();
        m_pauseBtn->setText(QStringLiteral("暂停"));
        if (m_queue.isEmpty()) {
            appendLog(QStringLiteral("导出完成但名单为空，无法互评"));
        } else {
            appendLog(QStringLiteral("列表更新完成，可点「开始互评」"));
        }
        updateStatusBar();
    }
}

void MainWindow::onStartClicked() {
    if (m_exporting || m_running) {
        return;
    }
    if (!m_followingStore->hasList()) {
        const auto ret = QMessageBox::question(
            this,
            QStringLiteral("导出蓝V关注列表"),
            QStringLiteral("蓝V关注列表为空，将自动导出蓝V关注列表，期间不能进行互动"),
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Ok);
        if (ret != QMessageBox::Ok) {
            appendLog(QStringLiteral("已取消导出"));
            return;
        }
        beginExport(true);
        return;
    }
    beginReplyFromCache();
}

void MainWindow::onUpdateFollowingClicked() {
    if (m_exporting || m_running) {
        return;
    }
    const auto ret = QMessageBox::question(
        this,
        QStringLiteral("更新蓝V关注列表"),
        QStringLiteral("更新蓝V关注列表，期间不能进行互动。\n\n"
                       "确定后重新导出并覆盖 data/following.json。"),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Ok);
    if (ret != QMessageBox::Ok) {
        appendLog(QStringLiteral("已取消更新列表"));
        return;
    }
    beginExport(false);
}

void MainWindow::onPauseClicked() {
    if (m_exporting) {
        m_exporting = false;
        m_startReplyAfterExport = false;
        m_phase = Phase::Idle;
        m_waitingLoad = false;
        m_queueTimer->stop();
        setControlsForIdle();
        m_pauseBtn->setText(QStringLiteral("暂停"));
        appendLog(QStringLiteral("已取消导出（未完成的列表不会写入）"));
        updateStatusBar();
        return;
    }
    m_running = false;
    m_phase = Phase::Idle;
    m_queueTimer->stop();
    setControlsForIdle();
    m_pauseBtn->setText(QStringLiteral("暂停"));
    appendLog(QStringLiteral("已暂停互评"));
    updateStatusBar();
}

void MainWindow::goNextAfter(int delayMs) {
    if (!m_running) {
        return;
    }
    m_phase = Phase::Cooling;
    updateStatusBar();
    m_queueTimer->start(delayMs);
}

void MainWindow::processQueue() {
    if (!m_running) {
        return;
    }
    if (m_replyStore->rollDayIfNeeded()) {
        appendLog(QStringLiteral("已跨日，今日回复列表已刷新"));
        updateStatusBar();
    }
    if (m_replyStore->todayCount() >= m_dailyLimitSpin->value()) {
        appendLog(QStringLiteral("已达每日上限 %1，停止").arg(m_dailyLimitSpin->value()));
        onPauseClicked();
        return;
    }
    openNextProfile();
}

void MainWindow::openNextProfile() {
    while (m_index < m_queue.size()) {
        const ReplyTarget& t = m_queue[m_index++];
        if (!m_ownHandle.isEmpty() && t.handle.compare(m_ownHandle, Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (m_replyStore->hasRepliedToday(t.handle)) {
            appendLog(QStringLiteral("跳过 @%1：今日已回复过").arg(t.handle));
            continue;
        }
        m_currentHandle = t.handle;
        m_currentPostId.clear();
        m_currentPostUrl.clear();
        m_phase = Phase::OpeningProfile;
        m_waitingLoad = true;
        updateStatusBar();
        appendLog(QStringLiteral("打开 @%1 (%2) 主页检测最新帖…").arg(t.handle, t.name));
        m_browser->LoadUrl(QStringLiteral("https://x.com/%1").arg(t.handle));
        return;
    }
    finishQueueExhausted();
}

void MainWindow::rememberScannedUser(const QString& handle) {
    const QString h = handle.trimmed();
    if (h.isEmpty()) {
        return;
    }
    m_lastScannedHandle = h;
    QSettings s(QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini"),
                QSettings::IniFormat);
    s.setValue(QStringLiteral("lastScannedHandle"), m_lastScannedHandle);
}

void MainWindow::applyResumeIndex() {
    m_index = 0;
    if (m_lastScannedHandle.isEmpty() || m_queue.isEmpty()) {
        return;
    }
    int found = -1;
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].handle.compare(m_lastScannedHandle, Qt::CaseInsensitive) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        appendLog(QStringLiteral("上次扫描用户 @%1 不在当前蓝V列表，从头开始")
                      .arg(m_lastScannedHandle));
        m_index = 0;
        return;
    }
    m_index = found + 1;
    if (m_index >= m_queue.size()) {
        appendLog(QStringLiteral("上次已扫到名单末尾（@%1），无可续扫用户")
                      .arg(m_lastScannedHandle));
        return;
    }
    appendLog(QStringLiteral("从 @%1 的下一位续扫（进度 %2/%3）")
                  .arg(m_lastScannedHandle)
                  .arg(m_index)
                  .arg(m_queue.size()));
}

void MainWindow::finishQueueExhausted() {
    appendLog(QStringLiteral("队列处理完毕。本次回复 %1，今日累计 %2")
                  .arg(m_sessionReplied)
                  .arg(m_replyStore->todayCount()));
    // 整轮扫完后清空断点，下次开始互评从头扫，避免一启动就再次「扫描完毕」
    m_lastScannedHandle.clear();
    QSettings s(QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini"),
                QSettings::IniFormat);
    s.setValue(QStringLiteral("lastScannedHandle"), QString());
    onPauseClicked();
    QMessageBox::information(this, QStringLiteral("互评完成"),
                             QStringLiteral("所有用户已扫描完毕"));
}

void MainWindow::onBrowserCreated() {
    m_statusLabel->setText(QStringLiteral("状态: 浏览器已就绪"));
    appendLog(QStringLiteral("CEF 浏览器已创建"));
}

void MainWindow::onLoadFinished(bool success) {
    if (!success) {
        appendLog(QStringLiteral("页面加载失败"));
        if (m_running && (m_phase == Phase::OpeningProfile || m_phase == Phase::OpeningPost)) {
            goNextAfter(2000);
        }
        return;
    }

    if (m_exporting && m_waitingLoad && m_phase == Phase::Exporting) {
        m_waitingLoad = false;
        QTimer::singleShot(1500, this, [this]() {
            if (!m_exporting || m_phase != Phase::Exporting) {
                return;
            }
            m_browser->ExecuteJavaScript(m_replier->getExportBlueFollowingScript());
        });
        return;
    }

    if (!m_running || !m_waitingLoad) {
        return;
    }
    m_waitingLoad = false;

    if (m_phase == Phase::OpeningProfile) {
        m_phase = Phase::Scanning;
        QTimer::singleShot(1200, this, [this]() {
            if (!m_running || m_phase != Phase::Scanning) {
                return;
            }
            m_browser->ExecuteJavaScript(m_replier->getScanLatestPostScript(effectiveKeywords()));
        });
        return;
    }
    if (m_phase == Phase::OpeningPost) {
        m_phase = Phase::Replying;
        QTimer::singleShot(1500, this, [this]() {
            if (!m_running || m_phase != Phase::Replying) {
                return;
            }
            m_browser->ExecuteJavaScript(m_replier->getReplyScript(effectiveReplyText()));
        });
    }
}

void MainWindow::onUrlChanged(const QString& url) {
    m_statusLabel->setText(QStringLiteral("状态: %1").arg(url));
}

void MainWindow::onJsResult(const QString& result) {
    const auto doc = QJsonDocument::fromJson(result.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();
    const QString op = obj.value(QStringLiteral("op")).toString();

    if (op == QStringLiteral("own_handle")) {
        m_ownHandle = obj.value(QStringLiteral("handle")).toString();
        appendLog(QStringLiteral("当前账号 @%1").arg(m_ownHandle));
        m_phase = Phase::Exporting;
        m_waitingLoad = true;
        return;
    }
    if (op == QStringLiteral("batch")) {
        if (!m_exporting) {
            return;
        }
        const QJsonArray users = obj.value(QStringLiteral("users")).toArray();
        for (const auto& v : users) {
            const QJsonObject u = v.toObject();
            ReplyTarget t;
            t.handle = u.value(QStringLiteral("handle")).toString();
            t.name = u.value(QStringLiteral("name")).toString();
            if (t.handle.isEmpty()) {
                continue;
            }
            bool exists = false;
            for (const auto& q : m_queue) {
                if (q.handle.compare(t.handle, Qt::CaseInsensitive) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                m_queue.append(t);
            }
        }
        updateStatusBar();
        return;
    }
    if (op == QStringLiteral("export_done")) {
        if (!m_exporting) {
            return;
        }
        appendLog(QStringLiteral("蓝V关注滚动采集完成，共 %1 人")
                      .arg(obj.value(QStringLiteral("total")).toInt()));
        finishExportAndSave();
        return;
    }
    if (op == QStringLiteral("error")) {
        appendLog(QStringLiteral("错误: %1").arg(obj.value(QStringLiteral("reason")).toString()));
        m_exporting = false;
        m_running = false;
        m_phase = Phase::Idle;
        setControlsForIdle();
        m_pauseBtn->setText(QStringLiteral("暂停"));
        return;
    }
    if (op == QStringLiteral("scan")) {
        if (!m_running) {
            return;
        }
        const bool ok = obj.value(QStringLiteral("ok")).toBool();
        const QString handle = obj.value(QStringLiteral("handle")).toString();
        const QString scanned = handle.isEmpty() ? m_currentHandle : handle;
        rememberScannedUser(scanned);
        if (!ok) {
            appendLog(QStringLiteral("@%1 跳过: %2 %3")
                          .arg(scanned)
                          .arg(obj.value(QStringLiteral("reason")).toString())
                          .arg(obj.value(QStringLiteral("content")).toString()));
            goNextAfter(1500);
            return;
        }
        m_currentHandle = scanned;
        m_currentPostId = obj.value(QStringLiteral("postId")).toString();
        m_currentPostUrl = obj.value(QStringLiteral("postUrl")).toString();
        appendLog(QStringLiteral("@%1 命中「%2」（%3），准备回复…")
                      .arg(scanned,
                           obj.value(QStringLiteral("matched")).toString(),
                           obj.value(QStringLiteral("postedAt")).toString()));
        m_phase = Phase::OpeningPost;
        m_waitingLoad = true;
        m_browser->LoadUrl(m_currentPostUrl);
        return;
    }
    if (op == QStringLiteral("reply")) {
        if (!m_running) {
            return;
        }
        const bool ok = obj.value(QStringLiteral("ok")).toBool();
        const QString handle = obj.value(QStringLiteral("handle")).toString(m_currentHandle);
        const QString postId = obj.value(QStringLiteral("postId")).toString(m_currentPostId);
        if (ok) {
            m_replyStore->markReplied(handle, postId);
            ++m_sessionReplied;
            appendLog(QStringLiteral("已回复 @%1（帖 %2）").arg(handle, postId));
            updateStatusBar();
            goNextAfter(m_cooldownSpin->value() * 1000);
        } else {
            appendLog(QStringLiteral("回复失败 @%1: %2")
                          .arg(handle, obj.value(QStringLiteral("reason")).toString()));
            goNextAfter(3000);
        }
    }
}

void MainWindow::updateStatusBar() {
    m_statsLabel->setText(QStringLiteral("队列: %1/%2 | 本次已回: %3 | 今日: %4/%5 | 缓存: %6")
                              .arg(m_index)
                              .arg(m_queue.size())
                              .arg(m_sessionReplied)
                              .arg(m_replyStore->todayCount())
                              .arg(m_dailyLimitSpin->value())
                              .arg(m_followingStore->count()));
}

void MainWindow::appendLog(const QString& message) {
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                  message);
    m_logTextEdit->append(line);
}
