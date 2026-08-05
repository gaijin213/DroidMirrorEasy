#pragma once

#include <QObject>
#include <QTimer>

#include "adbmanager.h"

// Implements the Android 11+ "Pair device with QR code" flow without any
// typing of IPs, ports or codes:
//   1. Generate a name + password and build the WIFI:T:ADB payload.
//   2. The phone scans the QR and starts advertising an _adb-tls-pairing
//      mDNS service named after the QR's S: field.
//   3. Poll `adb mdns services` for that instance, then `adb pair`.
//   4. Find the _adb-tls-connect service and `adb connect`.
class QrPairer : public QObject
{
    Q_OBJECT

public:
    explicit QrPairer(AdbManager *adb, QObject *parent = nullptr);

    bool isActive() const { return m_active; }
    QString payload() const { return m_payload; }
    QString statusText() const { return m_status; }

    void start();
    void cancel();

    // Non-QR fallback: find phones advertising wireless debugging over mDNS.
    static QList<MdnsService> discoverPhones(AdbManager *adb);

signals:
    void statusChanged(const QString &status);
    void payloadReady(const QString &payload);
    void paired(const QString &hostPort);
    void connected(const QString &serial);
    void failed(const QString &message);

private:
    void poll();
    void setStatus(const QString &status);

    AdbManager *m_adb;
    QTimer m_timer;
    QString m_name;
    QString m_password;
    QString m_payload;
    QString m_status;
    QString m_pairIp;
    bool m_active = false;
    bool m_paired = false;
    int m_elapsedSec = 0;
};
