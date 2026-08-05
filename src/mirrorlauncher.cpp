#include "mirrorlauncher.h"

MirrorLauncher::MirrorLauncher(QObject *parent)
    : QObject(parent)
{
}

bool MirrorLauncher::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

QStringList MirrorLauncher::buildArgs(const QString &serial, const MirrorSettings &s)
{
    QStringList args;
    args << "--serial" << serial;
    if (s.maxSize > 0)
        args << "--max-size" << QString::number(s.maxSize);
    if (s.maxFps > 0)
        args << "--max-fps" << QString::number(s.maxFps);
    if (!s.bitrate.isEmpty())
        args << "--video-bit-rate" << s.bitrate;
    if (!s.audio)
        args << "--no-audio";
    if (s.turnScreenOff)
        args << "--turn-screen-off";
    if (s.stayAwake)
        args << "--stay-awake";
    if (s.fullscreen)
        args << "--fullscreen";
    if (s.orientation >= 0)
        args << "--lock-video-orientation" << QString::number(s.orientation);
    return args;
}

bool MirrorLauncher::launch(const QString &serial, const MirrorSettings &settings, QString *error)
{
    if (isRunning())
        stop();

    m_process = new QProcess(this);
    m_serial = serial;

    const QStringList args = buildArgs(serial, settings);
    m_process->setProgram("scrcpy");
    m_process->setArguments(args);

    connect(m_process, &QProcess::errorOccurred, this, [this, error](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_serial.clear();
            emit mirrorStopped(QString(), -1,
                               "'scrcpy' was not found in PATH.\n\n"
                               "Install it with:\n  sudo pacman -S --needed android-tools scrcpy");
        }
        Q_UNUSED(error);
    });

    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                const QString serial = m_serial;
                m_serial.clear();
                m_process->deleteLater();
                m_process = nullptr;
                emit mirrorStopped(serial, exitCode, QString());
            });

    m_process->start();
    if (m_process->state() == QProcess::NotRunning) {
        if (error)
            *error = "Failed to start scrcpy.";
        return false;
    }

    emit mirrorStarted(serial);
    return true;
}

void MirrorLauncher::stop()
{
    if (!m_process)
        return;
    m_process->kill();
    m_process->waitForFinished(2000);
}
