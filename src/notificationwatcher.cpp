#include "notificationwatcher.h"

#include <QRegularExpression>

NotificationWatcher::NotificationWatcher(const QString &adb, const QString &serial, QObject *parent)
    : QObject(parent)
    , m_adb(adb)
    , m_serial(serial)
{
    m_timer.setInterval(3000);
    connect(&m_timer, &QTimer::timeout, this, &NotificationWatcher::poll);
    connect(&m_proc, &QProcess::finished, this, &NotificationWatcher::onFinished);
}

void NotificationWatcher::start()
{
    m_seen.clear();
    m_content.clear();
    m_priming = true;
    m_timer.start();
    poll();
}

void NotificationWatcher::stop()
{
    m_timer.stop();
    if (m_proc.state() != QProcess::NotRunning) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
    m_inFlight = false;
}

void NotificationWatcher::poll()
{
    if (m_inFlight || m_proc.state() != QProcess::NotRunning)
        return;
    m_inFlight = true;
    m_proc.start(m_adb, {QStringLiteral("-s"), m_serial, QStringLiteral("shell"),
                         QStringLiteral("dumpsys"), QStringLiteral("notification"),
                         QStringLiteral("--noredact")});
}

void NotificationWatcher::onFinished(int, QProcess::ExitStatus)
{
    m_inFlight = false;
    const QString out = QString::fromUtf8(m_proc.readAllStandardOutput());
    if (!out.isEmpty())
        parse(out);
}

QString NotificationWatcher::bundleValue(const QString &raw)
{
    QString v = raw.trimmed();
    if (v.isEmpty() || v == QLatin1String("null"))
        return QString();
    if (v.endsWith(')')) {
        const int open = v.indexOf('(');
        if (open >= 0)
            return v.mid(open + 1, v.length() - open - 2);
    }
    return v;
}

void NotificationWatcher::parse(const QString &out)
{
    static const QRegularExpression keyRe(QStringLiteral("pkg=([\\w.]+).*?key=(\\S+?):"));
    static const QRegularExpression titleRe(QStringLiteral("^android\\.title=(.*)$"));
    static const QRegularExpression textRe(QStringLiteral("^android\\.text=(.*)$"));

    QString curKey;
    QString curPkg;
    QString curTitle;
    QString curText;

    auto flush = [&]() {
        if (curKey.isEmpty())
            return;
        const QByteArray combined = (curTitle + QLatin1Char('\n') + curText).toUtf8();
        const bool hasContent = !combined.trimmed().isEmpty();

        if (m_priming || !hasContent) {
            m_seen.insert(curKey);
            if (hasContent)
                m_content.insert(curKey, combined);
        } else if (!m_seen.contains(curKey)) {
            m_seen.insert(curKey);
            m_content.insert(curKey, combined);
            emit phoneNotification({curPkg, curTitle, curText});
        } else if (m_content.value(curKey) != combined) {
            m_content.insert(curKey, combined);
            emit phoneNotification({curPkg, curTitle, curText});
        }

        curKey.clear();
        curPkg.clear();
        curTitle.clear();
        curText.clear();
    };

    const QStringList lines = out.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        const QRegularExpressionMatch m = keyRe.match(raw);
        if (m.hasMatch() && line.startsWith(QLatin1String("NotificationRecord"))) {
            flush();
            curPkg = m.captured(1);
            curKey = m.captured(2);
            continue;
        }
        if (curKey.isEmpty())
            continue;

        const QRegularExpressionMatch tm = titleRe.match(line);
        if (tm.hasMatch()) {
            curTitle = bundleValue(tm.captured(1));
            continue;
        }
        const QRegularExpressionMatch txm = textRe.match(line);
        if (txm.hasMatch())
            curText = bundleValue(txm.captured(1));
    }
    flush();
    m_priming = false;
}
