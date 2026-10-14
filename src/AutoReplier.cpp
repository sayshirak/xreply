#include "AutoReplier.h"

#include <QJsonArray>
#include <QJsonDocument>

AutoReplier::AutoReplier(QObject* parent)
    : QObject(parent) {
}

QString AutoReplier::getExportBlueFollowingScript() const {
    return QString::fromUtf8(R"JS(
(function() {
    function emit(obj) {
        console.log('[JSRESULT]' + JSON.stringify(obj));
    }
    function log(m) { console.log('[XREPLY] ' + m); }

    if (window.xreplyExport && window.xreplyExport.running) return;

    if (!/^\/[A-Za-z0-9_]+\/following\/?$/.test(window.location.pathname)) {
        let tries = 0;
        const finder = setInterval(() => {
            tries++;
            const link = document.querySelector('a[data-testid="AppTabBar_Profile_Link"]');
            const handle = link ? (link.getAttribute('href') || '').replace(/^\//, '').split('/')[0] : '';
            if (handle) {
                clearInterval(finder);
                log('own handle=' + handle + ', goto following');
                emit({ op: 'own_handle', handle: handle });
                window.location.href = 'https://x.com/' + handle + '/following';
            } else if (tries > 40) {
                clearInterval(finder);
                emit({ op: 'error', reason: 'no_handle' });
            }
        }, 500);
        return;
    }

    const state = window.xreplyExport = { running: true, seen: new Set(), idleTicks: 0, lastY: -1, lastCount: 0 };

    function isBlueV(cell) {
        return !!(cell.querySelector('[data-testid="icon-verified"]') ||
                  cell.querySelector('svg[aria-label="Verified account"]') ||
                  cell.querySelector('svg[aria-label="已认证帐号"]') ||
                  cell.querySelector('[aria-label="Verified account"]') ||
                  cell.querySelector('[aria-label="已认证帐号"]'));
    }

    function parseCell(cell) {
        let handle = '';
        let name = '';
        for (const link of cell.querySelectorAll('a[href^="/"]')) {
            const href = link.getAttribute('href') || '';
            if (!/^\/[A-Za-z0-9_]+$/.test(href)) continue;
            if (!handle) handle = href.substring(1);
            const t = (link.innerText || '').trim();
            if (!name && t && !t.startsWith('@')) name = t.split('\n')[0].trim();
        }
        if (!handle || !isBlueV(cell)) return null;
        return { handle: handle, name: name || handle };
    }

    function collect() {
        const batch = [];
        document.querySelectorAll('[data-testid="primaryColumn"] [data-testid="UserCell"]').forEach(cell => {
            const user = parseCell(cell);
            if (user && !state.seen.has(user.handle.toLowerCase())) {
                state.seen.add(user.handle.toLowerCase());
                batch.push(user);
            }
        });
        if (batch.length) emit({ op: 'batch', users: batch });
    }

    function clickRetry() {
        for (const btn of document.querySelectorAll('[role="button"]')) {
            const t = (btn.innerText || '').trim().toLowerCase();
            if (t === 'retry' || t === '重试') { btn.click(); return; }
        }
    }

    const observer = new MutationObserver(collect);
    observer.observe(document.body, { childList: true, subtree: true });

    const timer = setInterval(() => {
        collect();
        clickRetry();
        window.scrollBy(0, Math.floor(window.innerHeight * 0.8));
        const y = window.scrollY;
        if (state.seen.size === state.lastCount && Math.abs(y - state.lastY) < 2) state.idleTicks++;
        else state.idleTicks = 0;
        state.lastY = y;
        state.lastCount = state.seen.size;
        if (state.idleTicks >= 15) finish();
    }, 1200);

    function finish() {
        if (!state.running) return;
        state.running = false;
        clearInterval(timer);
        observer.disconnect();
        collect();
        emit({ op: 'export_done', total: state.seen.size });
        log('export done total=' + state.seen.size);
    }
    window.xreplyExportStop = finish;
    log('export blue following started');
    setTimeout(collect, 1500);
})();
)JS");
}

QString AutoReplier::getScanLatestPostScript(const QStringList& keywords) const {
    QJsonArray arr;
    for (const QString& k : keywords) {
        if (!k.trimmed().isEmpty()) {
            arr.append(k.trimmed());
        }
    }
    const QString kwJson = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));

    QString script = QString::fromUtf8(R"JS(
(function() {
    const keywords = )JS");
    script += kwJson;
    script += QString::fromUtf8(R"JS(;
    function emit(obj) { console.log('[JSRESULT]' + JSON.stringify(obj)); }
    function log(m) { console.log('[XREPLY] ' + m); }

    const path = window.location.pathname || '';
    const m = path.match(/^\/([A-Za-z0-9_]+)\/?$/);
    if (!m) {
        emit({ op: 'scan', ok: false, reason: 'not_profile', path: path });
        return;
    }
    const handle = m[1];
    let tries = 0;

    function isRetweet(article) {
        const social = article.querySelector('[data-testid="socialContext"]');
        if (!social) return false;
        const t = (social.innerText || '').toLowerCase();
        return t.includes('reposted') || t.includes('转帖') || t.includes('retweeted');
    }

    function authorOf(article) {
        for (const link of article.querySelectorAll('a[href^="/"]')) {
            const href = link.getAttribute('href') || '';
            if (/^\/[A-Za-z0-9_]+$/.test(href)) return href.substring(1);
        }
        return '';
    }

    function localDateStr(d) {
        const y = d.getFullYear();
        const m = String(d.getMonth() + 1).padStart(2, '0');
        const day = String(d.getDate()).padStart(2, '0');
        return y + '-' + m + '-' + day;
    }

    function postLocalDate(article) {
        const t = article.querySelector('time');
        const iso = t ? (t.getAttribute('datetime') || '') : '';
        if (!iso) return null;
        const d = new Date(iso);
        if (isNaN(d.getTime())) return null;
        return { iso: iso, localDate: localDateStr(d) };
    }

    function scan() {
        tries++;
        const today = localDateStr(new Date());
        const articles = document.querySelectorAll('article[data-testid="tweet"]');
        for (const article of articles) {
            if (isRetweet(article)) continue;
            const ah = authorOf(article);
            if (ah && ah.toLowerCase() !== handle.toLowerCase()) continue;

            const statusLink = article.querySelector('a[href*="/status/"]');
            const href = statusLink ? statusLink.getAttribute('href') : '';
            const postId = href ? (href.split('/status/')[1] || '').split('?')[0] : '';
            if (!postId) continue;

            const when = postLocalDate(article);
            if (!when) {
                emit({
                    op: 'scan', ok: false, handle: handle, postId: postId,
                    reason: 'no_time', content: '无法读取发布时间'
                });
                return;
            }
            if (when.localDate !== today) {
                emit({
                    op: 'scan', ok: false, handle: handle, postId: postId,
                    reason: 'not_today', postedAt: when.localDate,
                    content: '发帖日 ' + when.localDate + '（非今日 ' + today + '）'
                });
                return;
            }

            const contentDiv = article.querySelector('[data-testid="tweetText"]');
            const content = contentDiv ? (contentDiv.innerText || '') : '';
            let matched = null;
            for (const kw of keywords) {
                if (kw && content.toLowerCase().includes(String(kw).toLowerCase())) {
                    matched = kw;
                    break;
                }
            }
            if (!matched) {
                emit({
                    op: 'scan', ok: false, handle: handle, postId: postId,
                    reason: 'no_keyword', postedAt: when.localDate,
                    content: content.slice(0, 80)
                });
                return;
            }
            const postUrl = 'https://x.com' + href.split('?')[0];
            emit({
                op: 'scan', ok: true, handle: handle, postId: postId,
                postUrl: postUrl, matched: matched, postedAt: when.localDate,
                content: content.slice(0, 120)
            });
            return;
        }
        if (tries < 20) {
            setTimeout(scan, 800);
            return;
        }
        emit({ op: 'scan', ok: false, handle: handle, reason: 'no_post' });
    }

    log('scan latest for @' + handle + ' kw=' + JSON.stringify(keywords));
    setTimeout(scan, 1200);
})();
)JS");
    return script;
}

QString AutoReplier::getReplyScript(const QString& replyText) const {
    const QString textJson = QString::fromUtf8(
        QJsonDocument(QJsonArray{replyText}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);

    QString script = QString::fromUtf8(R"JS(
(function() {
    const replyText = )JS");
    script += textJson;
    script += QString::fromUtf8(R"JS(;
    function emit(obj) { console.log('[JSRESULT]' + JSON.stringify(obj)); }
    function log(m) { console.log('[XREPLY] ' + m); }

    const path = window.location.pathname || '';
    const pm = path.match(/\/status\/(\d+)/);
    const postId = pm ? pm[1] : '';
    const handleMatch = path.match(/^\/([A-Za-z0-9_]+)\/status\//);
    const handle = handleMatch ? handleMatch[1] : '';

    function findReplyButton() {
        const btns = document.querySelectorAll('[data-testid="reply"]');
        if (btns.length) return btns[0];
        for (const b of document.querySelectorAll('[role="button"]')) {
            const al = (b.getAttribute('aria-label') || '').toLowerCase();
            if (al.includes('reply') || al.includes('回复')) return b;
        }
        return null;
    }

    function findEditor() {
        return document.querySelector('[data-testid="tweetTextarea_0"]') ||
               document.querySelector('div[role="textbox"][data-testid^="tweetTextarea"]') ||
               document.querySelector('div.public-DraftEditor-content[contenteditable="true"]') ||
               document.querySelector('div[contenteditable="true"][role="textbox"]');
    }

    function findSendButton() {
        const b = document.querySelector('[data-testid="tweetButton"]') ||
                  document.querySelector('[data-testid="tweetButtonInline"]');
        if (b) return b;
        for (const btn of document.querySelectorAll('[role="button"]')) {
            const t = (btn.innerText || '').trim();
            if (t === 'Reply' || t === '回复' || t === 'Post' || t === '发帖') return btn;
        }
        return null;
    }

    function setEditorText(editor, text) {
        editor.focus();
        try {
            document.execCommand('selectAll', false, null);
            document.execCommand('insertText', false, text);
        } catch (e) {}
        editor.dispatchEvent(new InputEvent('input', { bubbles: true, data: text, inputType: 'insertText' }));
        editor.dispatchEvent(new Event('change', { bubbles: true }));
    }

    let step = 0;
    let tries = 0;
    const timer = setInterval(() => {
        tries++;
        if (tries > 40) {
            clearInterval(timer);
            emit({ op: 'reply', ok: false, handle: handle, postId: postId, reason: 'timeout' });
            return;
        }
        if (step === 0) {
            const rb = findReplyButton();
            if (!rb) return;
            rb.click();
            step = 1;
            log('clicked reply');
            return;
        }
        if (step === 1) {
            const ed = findEditor();
            if (!ed) return;
            setEditorText(ed, replyText);
            step = 2;
            log('filled reply text');
            return;
        }
        if (step === 2) {
            const send = findSendButton();
            if (!send) return;
            const disabled = send.getAttribute('aria-disabled') === 'true' || send.hasAttribute('disabled');
            if (disabled) return;
            send.click();
            clearInterval(timer);
            emit({ op: 'reply', ok: true, handle: handle, postId: postId });
            log('reply sent');
        }
    }, 500);
})();
)JS");
    return script;
}
