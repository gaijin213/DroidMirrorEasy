#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>

struct PhoneNotification
{
    QString package;
    QString title;
    QString text;
};

class NotificationWatcher : public QObject
{
    Q_OBJECT

public:
    NotificationWatcher(const QString &adb, const QString &serial, QObject *parent = nullptr);

    void start();
    void stop();
    bool isRunning() const { return m_timer.isActive(); }
    QString serial() const { return m_serial; }

signals:
    void phoneNotification(const PhoneNotification &notification);

private slots:
    void poll();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    void parse(const QString &out);
    static QString bundleValue(const QString &raw);

    QString m_adb;
    QString m_serial;
    QTimer m_timer;
    QProcess m_proc;
    bool m_inFlight = false;
    bool m_priming = true;
    QSet<QString> m_seen;
    QHash<QString, QByteArray> m_content;
};
