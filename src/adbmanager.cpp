#include "adbmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

AdbManager::AdbManager()
{
    m_adb = resolveAdb();
}

QString AdbManager::resolveAdb()
{
    const QString override = qEnvironmentVariable("DROIDMIRROR_ADB");
    if (!override.isEmpty())
        return override;

    const QStringList candidates = {
        QDir::homePath() + "/android-sdk/platform-tools/adb",
        "/opt/android-sdk/platform-tools/adb",
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }

#ifdef Q_OS_WIN
    return "adb.exe";
#else
    return "adb";
#endif
}

QString AdbManager::adbOrigin() const
{
    if (m_adb.startsWith('/'))
        return m_adb;
    return "PATH (" + m_adb + ")";
}

bool AdbManager::isAvailable(QString *error) const
{
    if (m_adb.startsWith('/') && QFileInfo(m_adb).isExecutable())
        return true;

    QProcess which;
    which.start("sh", {"-c", "command -v " + m_adb});
    which.waitForFinished(3000);
    if (which.exitCode() != 0 || which.readAllStandardOutput().trimmed().isEmpty()) {
        if (error)
            *error = QString("'%1' was not found in PATH.\n\n"
                             "Install it with:\n  sudo pacman -S --needed android-tools scrcpy\n"
                             "or put Google's platform-tools in ~/android-sdk/platform-tools/")
                         .arg(m_adb);
        return false;
    }
    return true;
}

QString AdbManager::runSync(const QStringList &args, int timeoutMs, QString *error) const
{
    QProcess p;
    p.start(m_adb, args);
    p.setReadChannel(QProcess::StandardOutput);
    if (!p.waitForStarted(5000)) {
        if (error)
            *error = "Failed to start adb: " + p.errorString();
        return QString();
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        if (error)
            *error = "adb timed out: " + args.join(' ');
        return QString();
    }
    QString out = p.readAllStandardOutput();
    QString err = p.readAllStandardError();
    if (error && p.exitCode() != 0)
        *error = err.trimmed().isEmpty() ? out.trimmed() : err.trimmed();
    return out;
}

DeviceInfo AdbManager::parseDevicesLine(const QString &line)
{
    DeviceInfo d;
    const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return d;
    d.serial = parts.value(0);
    d.state = parts.value(1);

    static const QRegularExpression re("(product|model|device):([\\w\\s-]+)");
    QRegularExpressionMatchIterator it = re.globalMatch(line);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        const QString key = m.captured(1);
        const QString value = m.captured(2).trimmed();
        if (key == "product")
            d.product = value;
        else if (key == "model")
            d.model = value;
        else if (key == "device")
            d.device = value;
    }
    return d;
}

QList<DeviceInfo> AdbManager::devices() const
{
    QList<DeviceInfo> result;
    const QString out = runSync({"devices", "-l"});
    const QStringList lines = out.split('\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith("List of devices"))
            continue;
        const DeviceInfo d = parseDevicesLine(line);
        if (!d.serial.isEmpty())
            result.append(d);
    }
    return result;
}

bool AdbManager::pairDevice(const QString &hostPort, const QString &code, QString *error)
{
    QProcess p;
    p.start(m_adb, {"pair", hostPort});
    p.setReadChannel(QProcess::StandardOutput);
    if (!p.waitForStarted(5000)) {
        if (error)
            *error = "Failed to start adb pair: " + p.errorString();
        return false;
    }

    p.write(code.toUtf8() + "\n");
    p.closeWriteChannel();
    if (!p.waitForFinished(15000)) {
        p.kill();
        p.waitForFinished(1000);
        if (error)
            *error = "adb pair timed out.";
        return false;
    }
    const QString out = p.readAllStandardOutput();
    const QString err = p.readAllStandardError();
    const QString all = out + err;

    if (p.exitCode() != 0 || all.contains("failed", Qt::CaseInsensitive) ||
        all.contains("error", Qt::CaseInsensitive)) {
        if (error)
            *error = all.trimmed();
        return false;
    }
    return true;
}

bool AdbManager::connectDevice(const QString &hostPort, QString *error)
{
    const QString out = runSync({"connect", hostPort}, 10000, error);
    if (error && !error->isEmpty())
        return false;
    if (out.contains("connected", Qt::CaseInsensitive))
        return true;
    if (error)
        *error = out.trimmed();
    return false;
}

bool AdbManager::disconnectDevice(const QString &serial, QString *error)
{
    runSync({"disconnect", serial}, 5000, error);
    return error == nullptr || error->isEmpty();
}

bool AdbManager::killServer(QString *error)
{
    runSync({"kill-server"}, 5000, error);
    return error == nullptr || error->isEmpty();
}

QList<MdnsService> AdbManager::mdnsServices() const
{
    QList<MdnsService> result;
    const QString out = runSync({"mdns", "services"});
    const QStringList lines = out.split('\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith("List of discovered"))
            continue;
        QStringList parts;
        if (line.contains('\t'))
            parts = line.split('\t');
        else
            parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 3)
            continue;
        const QString instance = parts.at(0);
        const QString svc = parts.at(1);
        if (!svc.startsWith("_adb-tls-"))
            continue;
        const QString endpoint = parts.at(2);
        const int colon = endpoint.lastIndexOf(':');
        if (colon <= 0)
            continue;
        MdnsService s;
        s.instance = instance;
        s.serviceType = svc;
        s.ip = endpoint.left(colon);
        s.port = endpoint.mid(colon + 1).toInt();
        if (s.port > 0)
            result.append(s);
    }
    return result;
}

bool AdbManager::ensureMdns(QString *error)
{
    QString out = runSync({"mdns", "check"}, 5000, error);
    if (out.contains("daemon version", Qt::CaseInsensitive))
        return true;
    runSync({"kill-server"}, 5000, nullptr);
    runSync({"start-server"}, 8000, nullptr);
    out = runSync({"mdns", "check"}, 5000, error);
    if (out.contains("daemon version", Qt::CaseInsensitive))
        return true;
    QString msg = out.trimmed();
    if (msg.isEmpty() && error && !error->isEmpty())
        msg = *error;
    if (msg.isEmpty())
        msg = "adb mDNS is unavailable. Install Google's platform-tools (or update android-tools).";
    if (error)
        *error = msg;
    return false;
}
