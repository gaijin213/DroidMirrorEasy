#include "qrpairer.h"

#include <QRandomGenerator>

namespace {

const QString kPairService = "_adb-tls-pairing";
const QString kConnectService = "_adb-tls-connect";
const int kTimeoutSec = 180;
const QString kAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";

QString randomString(int length)
{
    QString out;
    out.reserve(length);
    for (int i = 0; i < length; ++i)
        out.append(kAlphabet.at(QRandomGenerator::global()->bounded(kAlphabet.size())));
    return out;
}

} // namespace

QrPairer::QrPairer(AdbManager *adb, QObject *parent)
    : QObject(parent)
    , m_adb(adb)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &QrPairer::poll);
}

void QrPairer::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged(status);
}

void QrPairer::start()
{
    cancel();
    m_name = QString("droidmirror-%1").arg(randomString(6));
    m_password = randomString(12);
    m_payload = QString("WIFI:T:ADB;S:%1;P:%2;;").arg(m_name, m_password);
    m_paired = false;
    m_active = true;
    m_elapsedSec = 0;

    setStatus("Showing QR code. Scan it with your phone (Wireless debugging -> "
              "\"Pair device with QR code\").");
    emit payloadReady(m_payload);
    m_timer.start();
}

void QrPairer::cancel()
{
    m_timer.stop();
    m_active = false;
    m_payload.clear();
}

void QrPairer::poll()
{
    if (!m_active)
        return;

    if (++m_elapsedSec > kTimeoutSec) {
        m_timer.stop();
        m_active = false;
        emit failed("Timed out waiting for your phone to scan the QR.\n\n"
                    "Make sure both devices are on the same Wi-Fi network and that "
                    "Wireless debugging is ON. Then press the button and scan again.");
        return;
    }

    const QList<MdnsService> services = m_adb->mdnsServices();

    QList<MdnsService> pairing, connect;
    for (const MdnsService &s : services) {
        if (s.isPairing())
            pairing.append(s);
        else if (s.isConnect())
            connect.append(s);
    }

    if (!m_paired) {
        MdnsService match;
        bool found = false;
        for (const MdnsService &s : pairing) {
            if (s.instance == m_name) {
                match = s;
                found = true;
                break;
            }
        }
        if (!found && pairing.size() == 1)
            match = pairing.first(), found = true;

        if (found) {
            setStatus(QString("Phone found (%1). Pairing...").arg(match.hostPort()));
            QString err;
            if (m_adb->pairDevice(match.hostPort(), m_password, &err)) {
                m_paired = true;
                m_pairIp = match.ip;
                setStatus("Paired! Connecting...");
                emit paired(match.hostPort());
            } else {
                m_timer.stop();
                m_active = false;
                emit failed("Pairing failed:\n\n" + err);
                return;
            }
        }
    }

    if (m_paired) {
        MdnsService target;
        bool found = false;
        for (const MdnsService &s : connect) {
            if (!found && (s.instance == m_name || s.ip == m_pairIp)) {
                target = s;
                found = true;
            }
        }
        if (!found && connect.size() == 1)
            target = connect.first(), found = true;

        if (found) {
            QString err;
            if (m_adb->connectDevice(target.hostPort(), &err)) {
                m_timer.stop();
                m_active = false;
                emit connected(target.hostPort());
            }
        } else {
            const QList<DeviceInfo> devices = m_adb->devices();
            for (const DeviceInfo &d : devices) {
                if (d.state == "device" && d.serial.contains("_adb-tls-connect")) {
                    m_timer.stop();
                    m_active = false;
                    emit connected(d.serial);
                    return;
                }
            }
        }
    }
}

QList<MdnsService> QrPairer::discoverPhones(AdbManager *adb)
{
    return adb->mdnsServices();
}
