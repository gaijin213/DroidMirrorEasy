#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QTemporaryFile>
#include <QFile>
#include <QDir>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_pairer(&m_adb)
{
    setWindowTitle("DroidMirror Easy - Wireless Android Mirror");
    resize(900, 680);
    buildUi();
    buildConnections();

    m_pollTimer.setInterval(5000);
    m_pollTimer.start();

    refreshDevices();
    showStatus("Ready. Open the 'Easy connect' tab and press the button.");
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    auto *tabs = new QTabWidget(this);

    // ================= Easy tab =================
    auto *easy = new QWidget;
    auto *easyLayout = new QVBoxLayout(easy);
    easyLayout->setSpacing(12);

    auto *intro = new QLabel(
        "Connect your Android phone with ONE scan - no IPs, no ports, no codes to type.\n"
        "\n"
        "1. On your phone: <b>Settings</b> → <b>Developer options</b> → <b>Wireless debugging</b> → ON.\n"
        "   (No Developer options? Tap <b>About phone</b> → <b>Build number</b> 7 times.)\n"
        "2. Tap <b>Pair device with QR code</b> and scan the code below.\n"
        "3. Done! Press <b>Mirror phone</b>.", this);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    easyLayout->addWidget(intro);

    m_qrLabel = new QLabel(this);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setMinimumSize(320, 320);
    m_qrLabel->setStyleSheet("background:white; border:1px solid #888;");
    m_qrLabel->setText("QR code appears here.\n\nPress the green button below.");
    m_qrLabel->setWordWrap(true);
    easyLayout->addWidget(m_qrLabel, 1);

    auto *btnRow = new QHBoxLayout;
    m_startQrBtn = new QPushButton("Show QR code", this);
    m_startQrBtn->setStyleSheet("font-size:15px; padding:10px;");
    m_codeBtn = new QPushButton("No camera? Type the 6-digit code instead", this);
    btnRow->addWidget(m_startQrBtn, 2);
    btnRow->addWidget(m_codeBtn, 2);
    easyLayout->addLayout(btnRow);

    m_easyStatus = new QLabel("Waiting to start...", this);
    m_easyStatus->setWordWrap(true);
    m_easyStatus->setTextFormat(Qt::RichText);
    easyLayout->addWidget(m_easyStatus);

    m_notifyDesktop = new QCheckBox(
        "Forward phone notifications to the desktop (calls, messages, apps)", this);
    easyLayout->addWidget(m_notifyDesktop);

    auto *mirrorRow = new QHBoxLayout;
    m_mirrorBtn = new QPushButton("Mirror phone", this);
    m_mirrorBtn->setEnabled(false);
    m_mirrorBtn->setStyleSheet("font-size:16px; padding:12px; font-weight:bold;");
    m_stopBtn = new QPushButton("Stop mirroring", this);
    m_stopBtn->setEnabled(false);
    mirrorRow->addWidget(m_mirrorBtn, 3);
    mirrorRow->addWidget(m_stopBtn, 1);
    easyLayout->addLayout(mirrorRow);
    tabs->addTab(easy, "Easy connect");

    // ================= Advanced tab =================
    auto *adv = new QWidget;
    auto *advLayout = new QVBoxLayout(adv);

    auto *devicesBox = new QGroupBox("Connected devices");
    auto *devicesLayout = new QVBoxLayout(devicesBox);
    m_devicesTable = new QTableWidget(0, 3, this);
    m_devicesTable->setHorizontalHeaderLabels({"Model", "Serial", "State"});
    m_devicesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_devicesTable->horizontalHeader()->setStretchLastSection(true);
    m_devicesTable->verticalHeader()->setVisible(false);
    m_devicesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_devicesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_devicesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    devicesLayout->addWidget(m_devicesTable);

    auto *devBtnRow = new QHBoxLayout;
    m_refreshBtn = new QPushButton("Refresh", this);
    m_disconnectBtn = new QPushButton("Disconnect", this);
    m_disconnectBtn->setEnabled(false);
    devBtnRow->addWidget(m_refreshBtn);
    devBtnRow->addWidget(m_disconnectBtn);
    devBtnRow->addStretch();
    devicesLayout->addLayout(devBtnRow);
    advLayout->addWidget(devicesBox);

    auto *manualBox = new QGroupBox("Manual wireless connect (advanced)");
    auto *manualLayout = new QFormLayout(manualBox);
    auto *pairRow = new QHBoxLayout;
    m_pairHost = new QLineEdit(this);
    m_pairHost->setPlaceholderText("IP:port (from 'Pair device with pairing code')");
    m_pairCode = new QLineEdit(this);
    m_pairCode->setPlaceholderText("6-digit code");
    m_pairCode->setFixedWidth(90);
    m_pairBtn = new QPushButton("Pair", this);
    pairRow->addWidget(m_pairHost, 3);
    pairRow->addWidget(m_pairCode);
    pairRow->addWidget(m_pairBtn);
    manualLayout->addRow("Pair:", pairRow);

    auto *connectRow = new QHBoxLayout;
    m_connectHost = new QLineEdit(this);
    m_connectHost->setPlaceholderText("IP:port (from 'IP address & Port')");
    m_connectBtn = new QPushButton("Connect", this);
    connectRow->addWidget(m_connectHost, 3);
    connectRow->addWidget(m_connectBtn);
    manualLayout->addRow("Connect:", connectRow);
    advLayout->addWidget(manualBox);

    auto *mirrorBox = new QGroupBox("Mirror settings");
    auto *mirrorLayout = new QFormLayout(mirrorBox);

    m_maxSize = new QComboBox(this);
    m_maxSize->addItem("Original (no limit)", 0);
    m_maxSize->addItem("2560 (2K)", 2560);
    m_maxSize->addItem("1920 (Full HD)", 1920);
    m_maxSize->addItem("1600", 1600);
    m_maxSize->addItem("1280 (720p)", 1280);
    mirrorLayout->addRow("Max resolution:", m_maxSize);

    m_maxFps = new QSpinBox(this);
    m_maxFps->setRange(0, 144);
    m_maxFps->setSpecialValueText("Auto");
    mirrorLayout->addRow("Max FPS:", m_maxFps);

    m_bitrate = new QComboBox(this);
    m_bitrate->addItems({"2M", "4M", "8M", "12M", "16M", "20M", "32M", "40M"});
    mirrorLayout->addRow("Video bitrate:", m_bitrate);

    m_orientation = new QComboBox(this);
    m_orientation->addItem("Automatic (follow device)", -1);
    m_orientation->addItem("Lock: 0 deg (portrait)", 0);
    m_orientation->addItem("Lock: 90 deg", 1);
    m_orientation->addItem("Lock: 180 deg", 2);
    m_orientation->addItem("Lock: 270 deg", 3);
    mirrorLayout->addRow("Orientation:", m_orientation);

    m_audio = new QCheckBox("Forward device audio", this);
    m_turnScreenOff = new QCheckBox("Turn device screen off while mirroring", this);
    m_stayAwake = new QCheckBox("Keep device awake", this);
    m_fullscreen = new QCheckBox("Start fullscreen", this);
    mirrorLayout->addRow(QString(), m_audio);
    mirrorLayout->addRow(QString(), m_turnScreenOff);
    mirrorLayout->addRow(QString(), m_stayAwake);
    mirrorLayout->addRow(QString(), m_fullscreen);
    advLayout->addWidget(mirrorBox);
    advLayout->addStretch();
    tabs->addTab(adv, "Devices & settings (advanced)");

    root->addWidget(tabs);

    m_status = new QLabel(this);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);

    setCentralWidget(central);

    QSettings s;
    m_pairHost->setText(s.value("lastPairHost").toString());
    m_pairCode->setText(s.value("lastPairCode").toString());
    m_connectHost->setText(s.value("lastConnectHost").toString());
    m_maxSize->setCurrentIndex(qMax(0, m_maxSize->findData(s.value("maxSize", 1920).toInt())));
    m_maxFps->setValue(s.value("maxFps", 60).toInt());
    m_bitrate->setCurrentText(s.value("bitrate", "8M").toString());
    m_orientation->setCurrentIndex(qMax(0, m_orientation->findData(s.value("orientation", -1).toInt())));
    m_audio->setChecked(s.value("audio", true).toBool());
    m_turnScreenOff->setChecked(s.value("turnScreenOff", false).toBool());
    m_stayAwake->setChecked(s.value("stayAwake", false).toBool());
    m_fullscreen->setChecked(s.value("fullscreen", false).toBool());
    m_notifyDesktop->setChecked(s.value("notifyDesktop", true).toBool());
}

void MainWindow::buildConnections()
{
    connect(m_startQrBtn, &QPushButton::clicked, this, &MainWindow::onStartQrClicked);
    connect(m_codeBtn, &QPushButton::clicked, this, &MainWindow::onPairByCodeClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(&m_pollTimer, &QTimer::timeout, this, &MainWindow::refreshDevices);
    connect(m_pairBtn, &QPushButton::clicked, this, &MainWindow::onManualPairClicked);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_mirrorBtn, &QPushButton::clicked, this, &MainWindow::onMirrorClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_devicesTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onSelectionChanged);
    connect(&m_launcher, &MirrorLauncher::mirrorStopped, this, &MainWindow::onMirrorStopped);
    connect(m_notifyDesktop, &QCheckBox::toggled, this, &MainWindow::onNotifyToggled);

    connect(&m_pairer, &QrPairer::statusChanged, this, &MainWindow::onStatusChanged);
    connect(&m_pairer, &QrPairer::payloadReady, this, &MainWindow::onPayloadReady);
    connect(&m_pairer, &QrPairer::paired, this, &MainWindow::onPaired);
    connect(&m_pairer, &QrPairer::connected, this, &MainWindow::onConnected);
    connect(&m_pairer, &QrPairer::failed, this, &MainWindow::onQrFailed);
}

void MainWindow::onStartQrClicked()
{
    QString err;
    if (!m_adb.isAvailable(&err)) {
        showError("Missing adb", err);
        return;
    }
    if (!ensureMdnsOrWarn())
        return;
    m_pairer.start();
    m_startQrBtn->setText("Show a new QR code");
}

void MainWindow::onPayloadReady(const QString &payload)
{
    setQrImage(payload);
}

void MainWindow::setQrImage(const QString &payload)
{
    QTemporaryFile tmp(QDir::tempPath() + "/droidmirror-qr-XXXXXX.png");
    if (!tmp.open()) {
        showError("QR code", "Could not create a temporary file for the QR image.");
        return;
    }
    const QString path = tmp.fileName();
    tmp.close();

    QProcess qr;
    qr.start("qrencode", {"-s", "12", "-m", "3", "-o", path, payload});
    if (!qr.waitForStarted(3000) || !qr.waitForFinished(8000)) {
        showError("QR code", "The 'qrencode' tool is missing.\nInstall it: sudo pacman -S qrencode");
        return;
    }

    QPixmap pm(path);
    QFile::remove(path);
    if (pm.isNull()) {
        showError("QR code", "Could not render the QR code.");
        return;
    }
    const QPixmap scaled = pm.scaled(m_qrLabel->size() - QSize(8, 8),
                                    Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_qrLabel->setPixmap(scaled);
}

void MainWindow::onStatusChanged(const QString &status)
{
    m_easyStatus->setText(status);
    showStatus(status);
}

void MainWindow::onPaired(const QString &hostPort)
{
    showStatus(QString("Paired with %1").arg(hostPort));
}

void MainWindow::onConnected(const QString &serial)
{
    m_pendingSerial = serial;
    m_easyStatus->setText("<b>Connected!</b> Your phone is ready. Press the big button to mirror.");
    showStatus("Connected to " + serial + ". Press 'Mirror phone'.");
    m_mirrorBtn->setEnabled(true);
    refreshDevices();
    updateNotifier();
}

void MainWindow::onQrFailed(const QString &message)
{
    m_easyStatus->setText("<span style='color:#c0392b;'>" + message.toHtmlEscaped() + "</span>");
    m_startQrBtn->setText("Show QR code");
}

void MainWindow::onPairByCodeClicked()
{
    QString err;
    if (!m_adb.isAvailable(&err)) {
        showError("Missing adb", err);
        return;
    }
    if (!ensureMdnsOrWarn())
        return;

    const QList<MdnsService> services = m_adb.mdnsServices();
    MdnsService pairing;
    for (const MdnsService &s : services) {
        if (s.isPairing()) {
            pairing = s;
            break;
        }
    }
    if (pairing.port == 0) {
        showError("No phone found",
                  "No Android phone is advertising wireless debugging.\n\n"
                  "On the phone, open Settings → Developer options → Wireless debugging and "
                  "tap 'Pair device with pairing code'. Make sure it is ON and on the same Wi-Fi.");
        return;
    }

    bool ok = false;
    const QString code = QInputDialog::getText(this, "Enter pairing code",
                                               QString("Enter the 6-digit code shown on your phone.\n\n"
                                                       "Phone: %1 (%2)")
                                                   .arg(pairing.instance, pairing.hostPort()),
                                               QLineEdit::Normal, QString(), &ok);
    if (!ok || code.trimmed().isEmpty())
        return;

    showStatus(QString("Pairing with %1 ...").arg(pairing.hostPort()));
    if (!m_adb.pairDevice(pairing.hostPort(), code.trimmed(), &err)) {
        showError("Pairing failed", err);
        showStatus("Pairing failed.");
        return;
    }
    showStatus("Paired. Connecting...");

    QString connectTarget;
    for (const MdnsService &s : m_adb.mdnsServices()) {
        if (s.isConnect() && s.ip == pairing.ip) {
            connectTarget = s.hostPort();
            break;
        }
    }
    if (connectTarget.isEmpty() && !pairing.ip.isEmpty())
        connectTarget = pairing.ip + ":5555";

    if (!m_adb.connectDevice(connectTarget, &err)) {
        showError("Connect failed", err);
        showStatus("Paired, but auto-connect failed. See the advanced tab.");
        return;
    }
    showStatus("Connected!");
    m_mirrorBtn->setEnabled(true);
    m_pendingSerial = connectTarget;
    refreshDevices();
    updateNotifier();
}

bool MainWindow::ensureMdnsOrWarn()
{
    QString err;
    if (!m_adb.ensureMdns(&err)) {
        showError("Wireless discovery unavailable", err);
        return false;
    }
    return true;
}

void MainWindow::refreshDevices()
{
    QString err;
    if (!m_adb.isAvailable(&err)) {
        m_devicesTable->setRowCount(0);
        m_mirrorBtn->setEnabled(false);
        return;
    }

    const QList<DeviceInfo> devices = m_adb.devices();
    m_devicesTable->setRowCount(devices.size());
    for (int i = 0; i < devices.size(); ++i) {
        const DeviceInfo &d = devices.at(i);
        auto *model = new QTableWidgetItem(d.model.isEmpty() ? d.product : d.model);
        auto *serial = new QTableWidgetItem(d.serial);
        auto *state = new QTableWidgetItem(d.state);
        model->setData(Qt::UserRole, d.serial);
        m_devicesTable->setItem(i, 0, model);
        m_devicesTable->setItem(i, 1, serial);
        m_devicesTable->setItem(i, 2, state);
    }
    onSelectionChanged();
    if (m_notifier)
        updateNotifier();
}

void MainWindow::onManualPairClicked()
{
    QString err;
    if (!m_adb.isAvailable(&err)) {
        showError("Missing adb", err);
        return;
    }
    const QString host = m_pairHost->text().trimmed();
    const QString code = m_pairCode->text().trimmed();
    if (host.isEmpty() || code.isEmpty()) {
        showError("Pairing", "Enter the IP:port and the 6-digit pairing code from the phone.");
        return;
    }
    QSettings().setValue("lastPairHost", host);
    QSettings().setValue("lastPairCode", code);
    showStatus(QString("Pairing with %1 ...").arg(host));
    m_pairBtn->setEnabled(false);
    if (m_adb.pairDevice(host, code, &err)) {
        showStatus("Paired. Now enter the 'IP address & Port' and press Connect.");
    } else {
        showError("Pairing failed", err);
        showStatus("Pairing failed.");
    }
    m_pairBtn->setEnabled(true);
}

void MainWindow::onConnectClicked()
{
    QString err;
    if (!m_adb.isAvailable(&err)) {
        showError("Missing adb", err);
        return;
    }
    const QString host = m_connectHost->text().trimmed();
    if (host.isEmpty()) {
        showError("Connect", "Enter the IP:port from the phone's Wireless debugging screen.");
        return;
    }
    QSettings().setValue("lastConnectHost", host);
    showStatus(QString("Connecting to %1 ...").arg(host));
    m_connectBtn->setEnabled(false);
    if (m_adb.connectDevice(host, &err)) {
        showStatus("Connected.");
        m_mirrorBtn->setEnabled(true);
        m_pendingSerial = host;
    } else {
        showError("Connect failed", err);
        showStatus("Connect failed.");
    }
    m_connectBtn->setEnabled(true);
    refreshDevices();
    updateNotifier();
}

void MainWindow::onDisconnectClicked()
{
    const QString serial = selectedSerial();
    if (serial.isEmpty())
        return;
    QString err;
    if (!m_adb.disconnectDevice(serial, &err))
        showError("Disconnect", err);
    refreshDevices();
    updateNotifier();
}

void MainWindow::onMirrorClicked()
{
    QString serial = selectedSerial();
    if (serial.isEmpty() && !m_pendingSerial.isEmpty())
        serial = m_pendingSerial;
    if (serial.isEmpty()) {
        showError("Mirror", "No phone selected. Select a device in the list first.");
        return;
    }

    const MirrorSettings s = currentSettings();
    QString err;
    if (!m_launcher.launch(serial, s, &err)) {
        showError("Mirror", err.isEmpty() ? "Failed to start scrcpy." : err);
        return;
    }

    QSettings().setValue("maxSize", s.maxSize);
    QSettings().setValue("maxFps", s.maxFps);
    QSettings().setValue("bitrate", s.bitrate);
    QSettings().setValue("orientation", s.orientation);
    QSettings().setValue("audio", s.audio);
    QSettings().setValue("turnScreenOff", s.turnScreenOff);
    QSettings().setValue("stayAwake", s.stayAwake);
    QSettings().setValue("fullscreen", s.fullscreen);

    m_mirrorBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    showStatus(QString("Mirroring %1 (scrcpy). Press Stop to close.").arg(serial));
}

void MainWindow::onStopClicked()
{
    m_launcher.stop();
    m_mirrorBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    showStatus("Mirroring stopped.");
}

void MainWindow::onMirrorStopped(const QString &serial, int exitCode, const QString &reason)
{
    m_mirrorBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    if (!reason.isEmpty())
        showError("scrcpy", reason);
    showStatus(QString("Mirroring ended (exit %1).").arg(exitCode));
    Q_UNUSED(serial);
}

void MainWindow::onSelectionChanged()
{
    const bool has = m_devicesTable->currentRow() >= 0;
    m_disconnectBtn->setEnabled(has);
}

QString MainWindow::selectedSerial() const
{
    const int row = m_devicesTable->currentRow();
    if (row < 0)
        return QString();
    QTableWidgetItem *item = m_devicesTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

MirrorSettings MainWindow::currentSettings() const
{
    MirrorSettings s;
    s.maxSize = m_maxSize->currentData().toInt();
    s.maxFps = m_maxFps->value();
    s.bitrate = m_bitrate->currentText();
    s.orientation = m_orientation->currentData().toInt();
    s.audio = m_audio->isChecked();
    s.turnScreenOff = m_turnScreenOff->isChecked();
    s.stayAwake = m_stayAwake->isChecked();
    s.fullscreen = m_fullscreen->isChecked();
    return s;
}

void MainWindow::showError(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
}

void MainWindow::showStatus(const QString &message)
{
    m_status->setText(message);
}

void MainWindow::onNotifyToggled(bool checked)
{
    QSettings().setValue("notifyDesktop", checked);
    updateNotifier();
}

void MainWindow::updateNotifier()
{
    const bool want = m_notifyDesktop->isChecked();
    if (!want) {
        if (m_notifier) {
            m_notifier->stop();
            m_notifier->deleteLater();
            m_notifier = nullptr;
            showStatus("Phone notifications disabled.");
        }
        return;
    }

    QString serial = m_pendingSerial;
    if (serial.isEmpty()) {
        const QList<DeviceInfo> devices = m_adb.devices();
        for (const DeviceInfo &d : devices) {
            if (d.state == QLatin1String("device")) {
                serial = d.serial;
                break;
            }
        }
    }

    if (m_notifier && m_notifier->serial() == serial)
        return;

    if (m_notifier) {
        m_notifier->stop();
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }

    if (serial.isEmpty()) {
        showStatus("Phone notifications: connect a phone first (Easy tab or Advanced tab).");
        return;
    }

    m_notifier = new NotificationWatcher(m_adb.path(), serial, this);
    connect(m_notifier, &NotificationWatcher::phoneNotification,
            this, &MainWindow::onPhoneNotification);
    m_notifier->start();
    showStatus(QString("Forwarding notifications from %1 ...").arg(serial));
}

void MainWindow::onPhoneNotification(const PhoneNotification &n)
{
    showDesktopNotification(prettyPackage(n.package), n.title, n.text);
}

QString MainWindow::prettyPackage(const QString &package)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("com.android.phone"), QStringLiteral("Phone")},
        {QStringLiteral("com.android.server.telecom"), QStringLiteral("Phone")},
        {QStringLiteral("com.google.android.apps.messaging"), QStringLiteral("Messages")},
        {QStringLiteral("com.android.mms"), QStringLiteral("Messages")},
        {QStringLiteral("com.whatsapp"), QStringLiteral("WhatsApp")},
        {QStringLiteral("com.google.android.gm"), QStringLiteral("Gmail")},
        {QStringLiteral("org.telegram.messenger"), QStringLiteral("Telegram")},
        {QStringLiteral("com.instagram.android"), QStringLiteral("Instagram")},
        {QStringLiteral("com.facebook.katana"), QStringLiteral("Facebook")},
        {QStringLiteral("com.android.systemui"), QStringLiteral("System")},
    };
    if (names.contains(package))
        return names.value(package);

    const int dot = package.lastIndexOf('.');
    if (dot > 0 && dot < package.size() - 1) {
        QString last = package.mid(dot + 1);
        last[0] = last[0].toUpper();
        return last;
    }
    return package;
}

void MainWindow::showDesktopNotification(const QString &app, const QString &title, const QString &body)
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.Notifications"),
                         QStringLiteral("/org/freedesktop/Notifications"),
                         QStringLiteral("org.freedesktop.Notifications"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid())
        return;

    QList<QVariant> args;
    args << QStringLiteral("droidmirror-easy")
         << QVariant::fromValue(0u)
         << QStringLiteral("dialog-information")
         << title
         << (body.isEmpty() ? app : app + QLatin1String(": ") + body)
         << QStringList()
         << QVariantMap()
         << QVariant::fromValue(-1);
    iface.callWithArgumentList(QDBus::Block, QStringLiteral("Notify"), args);
}
