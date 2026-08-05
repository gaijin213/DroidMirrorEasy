#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

struct MirrorSettings
{
    int maxSize = 1920;
    int maxFps = 60;
    QString bitrate = "8M";
    bool audio = true;
    bool turnScreenOff = false;
    bool stayAwake = false;
    bool fullscreen = false;
    int orientation = -1;
};

class MirrorLauncher : public QObject
{
    Q_OBJECT

public:
    explicit MirrorLauncher(QObject *parent = nullptr);

    bool isRunning() const;
    QString deviceSerial() const { return m_serial; }

    bool launch(const QString &serial, const MirrorSettings &settings, QString *error);
    void stop();

    static QStringList buildArgs(const QString &serial, const MirrorSettings &settings);

signals:
    void mirrorStarted(const QString &serial);
    void mirrorStopped(const QString &serial, int exitCode, const QString &reason);

private:
    QProcess *m_process = nullptr;
    QString m_serial;
};
