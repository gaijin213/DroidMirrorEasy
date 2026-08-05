#pragma once

#include <QList>
#include <QString>

struct DeviceInfo
{
    QString serial;
    QString state;
    QString model;
    QString product;
    QString device;
};

struct MdnsService
{
    QString instance;
    QString serviceType;
    QString ip;
    int port = 0;

    bool isPairing() const { return serviceType.startsWith("_adb-tls-pairing"); }
    bool isConnect() const { return serviceType.startsWith("_adb-tls-connect"); }
    QString hostPort() const { return QString("%1:%2").arg(ip).arg(port); }
};

class AdbManager
{
public:
    AdbManager();

    bool isAvailable(QString *error = nullptr) const;
    QString path() const { return m_adb; }
    QString adbOrigin() const;

    QList<DeviceInfo> devices() const;
    QList<MdnsService> mdnsServices() const;
    bool ensureMdns(QString *error);

    bool pairDevice(const QString &hostPort, const QString &code, QString *error);
    bool connectDevice(const QString &hostPort, QString *error);
    bool disconnectDevice(const QString &serial, QString *error);
    bool killServer(QString *error);

    static DeviceInfo parseDevicesLine(const QString &line);

private:
    QString runSync(const QStringList &args, int timeoutMs = 8000, QString *error = nullptr) const;
    static QString resolveAdb();

    QString m_adb;
};
