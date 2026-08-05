#pragma once

#include <QMainWindow>
#include <QTimer>

#include "adbmanager.h"
#include "mirrorlauncher.h"
#include "notificationwatcher.h"
#include "qrpairer.h"

class QTableWidget;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshDevices();
    void onStartQrClicked();
    void onPairByCodeClicked();
    void onManualPairClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onMirrorClicked();
    void onStopClicked();
    void onMirrorStopped(const QString &serial, int exitCode, const QString &reason);
    void onSelectionChanged();
    void onStatusChanged(const QString &status);
    void onPaired(const QString &hostPort);
    void onConnected(const QString &serial);
    void onQrFailed(const QString &message);
    void onPayloadReady(const QString &payload);
    void onNotifyToggled(bool checked);
    void onPhoneNotification(const PhoneNotification &notification);

private:
    void buildUi();
    void buildConnections();
    void showError(const QString &title, const QString &message);
    void showStatus(const QString &message);
    QString selectedSerial() const;
    MirrorSettings currentSettings() const;
    void setQrImage(const QString &payload);
    bool ensureMdnsOrWarn();
    void updateNotifier();
    void showDesktopNotification(const QString &app, const QString &title, const QString &body);
    static QString prettyPackage(const QString &package);

    AdbManager m_adb;
    QrPairer m_pairer;
    MirrorLauncher m_launcher;
    NotificationWatcher *m_notifier = nullptr;
    QTimer m_pollTimer;

    // Easy tab
    QLabel *m_qrLabel = nullptr;
    QPushButton *m_startQrBtn = nullptr;
    QPushButton *m_codeBtn = nullptr;
    QPushButton *m_mirrorBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QLabel *m_easyStatus = nullptr;

    // Advanced tab
    QTableWidget *m_devicesTable = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QLineEdit *m_pairHost = nullptr;
    QLineEdit *m_pairCode = nullptr;
    QPushButton *m_pairBtn = nullptr;
    QLineEdit *m_connectHost = nullptr;
    QPushButton *m_connectBtn = nullptr;

    QComboBox *m_maxSize = nullptr;
    QSpinBox *m_maxFps = nullptr;
    QComboBox *m_bitrate = nullptr;
    QCheckBox *m_audio = nullptr;
    QCheckBox *m_turnScreenOff = nullptr;
    QCheckBox *m_stayAwake = nullptr;
    QCheckBox *m_fullscreen = nullptr;
    QComboBox *m_orientation = nullptr;
    QCheckBox *m_notifyDesktop = nullptr;

    QLabel *m_status = nullptr;
    QString m_pendingSerial;
};
