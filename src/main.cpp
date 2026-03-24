//+-----------------------------------------------------------------+
//|                                                  SSH Proxy Tray |
//|                                   Copyright (c) 2025, Arthur V. |
//|                                   https://github.com/arthur-cpp |
//+-----------------------------------------------------------------+
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QIcon>
#include <QTimer>
#include <QDir>
#include <QTextStream>
#include <QSettings>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QIntValidator>
#include <QFile>
#include <QMessageBox>
#include <QThread>

//+-----------------------------------------------------------------+
//| Global                                                          |
//+-----------------------------------------------------------------+
static bool proxyEnabled = false;
QSystemTrayIcon* globalTray = nullptr; // for access from functions
QSettings settings("ssh-proxy-tray", "ssh-proxy-tray");
//+-----------------------------------------------------------------+
//| Settings accessors                                              |
//+-----------------------------------------------------------------+
QString sshHost()            { return settings.value("ssh_host", "nl").toString(); }
int  socksPort()             { return settings.value("socks_port", 1080).toInt(); }
bool autoConnectSetting()    { return settings.value("auto_connect", false).toBool(); }
bool autoStartSetting()      { return settings.value("auto_start", false).toBool(); }
bool useSystemProxySetting() { return settings.value("use_system_proxy", true).toBool(); }

void setSshHost(const QString& host)       { settings.setValue("ssh_host", host); }
void setSocksPort(int port)                { settings.setValue("socks_port", port); }
void setAutoConnectSetting(bool enable)    { settings.setValue("auto_connect", enable); }
void setAutoStartSetting(bool enable)      { settings.setValue("auto_start", enable); }
void setUseSystemProxySetting(bool enable) { settings.setValue("use_system_proxy", enable); }
//+-----------------------------------------------------------------+
//| Forward declarations                                            |
//+-----------------------------------------------------------------+
bool isProxyRunning();
//+-----------------------------------------------------------------+
//| Autostart management                                            |
//+-----------------------------------------------------------------+
void setupAutostart(bool enable) {
    QString autostartDir = QDir::homePath() + "/.config/autostart";
    QString desktopFile = autostartDir + "/ssh-proxy-tray.desktop";
    
    if (enable) {
        QDir().mkpath(autostartDir);
        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n"
                << "Type=Application\n"
                << "Name=SSH Proxy Tray\n"
                << "Exec=" << QCoreApplication::applicationFilePath() << "\n"
                << "Icon=network-vpn-symbolic\n"
                << "X-GNOME-Autostart-enabled=true\n"
                << "NoDisplay=false\n"
                << "Terminal=false\n";
        }
    } else {
        QFile::remove(desktopFile);
    }
}
//+-----------------------------------------------------------------+
//| Core proxy control                                              |
//+-----------------------------------------------------------------+
bool enableProxy() {
    // check port availability
    QProcess checkPort;
    checkPort.start("ss", {"-tlnp"});
    checkPort.waitForFinished();
    QString portCheck = checkPort.readAllStandardOutput();
    if (portCheck.contains(QString(":%1 ").arg(socksPort()))) {
        // check if this is our SSH process or another one
        if (!isProxyRunning()) {
            if (globalTray) {
                globalTray->showMessage("SSH Proxy Error",
                                     QString("Port %1 is already in use").arg(socksPort()),
                                     QSystemTrayIcon::Critical, 5000);
            }
            return false;
        }
    }
    
    // start SSH tunnel
    QProcess sshProc;
    sshProc.start("ssh", {
        "-D", QString::number(socksPort()),
        "-f", "-N", "-q",
        sshHost()
    });
    sshProc.waitForFinished(10000); // 10 second timeout
    
    if (sshProc.exitCode() != 0) {
        QString error = sshProc.readAllStandardError();
        if (globalTray) {
            globalTray->showMessage("SSH Proxy Error",
                                   QString("Failed to connect to %1: %2")
                                   .arg(sshHost(), error.isEmpty() ? "Connection failed" : error),
                                   QSystemTrayIcon::Critical, 5000);
        }
        return false;
    }
    
    // small delay for SSH initialization
    QThread::msleep(500);
    
    // configure system proxy via gsettings only if option is enabled
    if (useSystemProxySetting()) {
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy", "mode", "manual"});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.http", "host", ""});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.http", "port", "0"});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.https", "host", ""});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.https", "port", "0"});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.ftp", "host", ""});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.ftp", "port", "0"});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.socks", "host", "127.0.0.1"});
        QProcess::execute("gsettings", {"set", "org.gnome.system.proxy.socks", "port", QString::number(socksPort())});
    }
    
    proxyEnabled = true;
    return true;
}
//+-----------------------------------------------------------------+
//| Disable proxy                                                   |
//+-----------------------------------------------------------------+
bool disableProxy(bool clearSystemProxy = true) {
    // disable system proxy if clearing is needed
    // clearSystemProxy is used when switching mode - need to clear old settings
    if (clearSystemProxy && useSystemProxySetting()) {
        // check current state in gsettings - if proxy was configured, clear it
        QProcess checkProc;
        checkProc.start("gsettings", {"get", "org.gnome.system.proxy", "mode"});
        checkProc.waitForFinished();
        QString mode = checkProc.readAllStandardOutput().trimmed();
        
        // if proxy was in manual mode, clear it
        if (mode == "'manual'" || mode == "\"manual\"") {
            QProcess::execute("gsettings", {"set", "org.gnome.system.proxy", "mode", "none"});
        }
    }
    
    // kill SSH process (always)
    QProcess killProc;
    killProc.start("pkill", {"-f", QString("ssh.*-D\\s+%1").arg(socksPort())});
    killProc.waitForFinished();
    
    proxyEnabled = false;
    return true;
}
//+-----------------------------------------------------------------+
//| Check proxy running                                             |
//+-----------------------------------------------------------------+
bool isProxyRunning() {
    // more accurate check via ss (socket statistics)
    QProcess proc;
    proc.start("ss", {"-tlnp"});
    proc.waitForFinished();
    QString output = proc.readAllStandardOutput();
    
    // check that port is listening and this is SSH process
    if (output.contains(QString(":%1 ").arg(socksPort()))) {
        // additional check via pgrep
        QProcess pgrepProc;
        pgrepProc.start("pgrep", {"-f", QString("ssh.*-D\\s+%1").arg(socksPort())});
        pgrepProc.waitForFinished();
        return (pgrepProc.exitCode() == 0);
    }
    return false;
}
//+-----------------------------------------------------------------+
//| Tray UI helpers                                                 |
//+-----------------------------------------------------------------+
void updateTray(QSystemTrayIcon& tray, QAction& toggleAction) {
    // always check actual state
    proxyEnabled = isProxyRunning();
    
    if (proxyEnabled) {
        QIcon icon = QIcon::fromTheme("network-vpn-symbolic");
        if (icon.isNull()) {
            icon = QIcon::fromTheme("network-vpn");
        }
        tray.setIcon(icon);
        toggleAction.setText("SSH Proxy Off");
        QString modeText = useSystemProxySetting() ? " (System)" : " (Local)";
        tray.setToolTip(QString("SSH Proxy: ON%1\nHost: %2:%3")
                       .arg(modeText, sshHost(), QString::number(socksPort())));
    } else {
        QIcon icon = QIcon::fromTheme("network-vpn-disconnected-symbolic");
        if (icon.isNull()) {
            icon = QIcon::fromTheme("network-offline");
        }
        tray.setIcon(icon);
        toggleAction.setText("SSH Proxy On");
        tray.setToolTip("SSH Proxy: OFF");
    }
}
//+-----------------------------------------------------------------+
//| Settings dialog                                                 |
//+-----------------------------------------------------------------+
class SettingsDialog : public QDialog {
public:
    SettingsDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("SSH Proxy Settings");
        auto* layout = new QFormLayout(this);

        hostEdit = new QLineEdit(sshHost());
        portEdit = new QLineEdit(QString::number(socksPort()));
        portEdit->setValidator(new QIntValidator(1, 65535, this));

        useSystemProxyCheck = new QCheckBox("Use as system proxy");
        useSystemProxyCheck->setChecked(useSystemProxySetting());
        useSystemProxyCheck->setToolTip("If enabled, proxy will be set as system proxy via gsettings.\n"
                                       "If disabled, only local SOCKS proxy will be available at 127.0.0.1:port");

        autoConnectCheck = new QCheckBox("Auto-connect");
        autoConnectCheck->setChecked(autoConnectSetting());

        autoStartCheck = new QCheckBox("Autostart");
        autoStartCheck->setChecked(autoStartSetting());

        layout->addRow("SSH Host:", hostEdit);
        layout->addRow("SOCKS Port:", portEdit);
        layout->addRow(useSystemProxyCheck);
        layout->addRow(autoConnectCheck);
        layout->addRow(autoStartCheck);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    QString host() const { return hostEdit->text(); }
    int port() const { return portEdit->text().toInt(); }
    bool useSystemProxy() const { return useSystemProxyCheck->isChecked(); }
    bool autoConnect() const { return autoConnectCheck->isChecked(); }
    bool autoStart() const { return autoStartCheck->isChecked(); }

private:
    QLineEdit* hostEdit;
    QLineEdit* portEdit;
    QCheckBox* useSystemProxyCheck;
    QCheckBox* autoConnectCheck;
    QCheckBox* autoStartCheck;
};
//+-----------------------------------------------------------------+
//| Main                                                            |
//+-----------------------------------------------------------------+
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning("System tray not available!");
        return 1;
    }

    QMenu menu;
    QAction toggleAction("SSH Proxy On");
    QAction useSystemProxyAction("Use as system proxy");
    QAction autoStartAction("Autostart");
    QAction autoConnectAction("Auto-connect");
    QAction settingsAction("Settings...");
    QAction quitAction("Quit");

    QSystemTrayIcon tray;
    globalTray = &tray; // save for access from functions
    
    proxyEnabled = isProxyRunning();
    updateTray(tray, toggleAction);

    useSystemProxyAction.setCheckable(true);
    useSystemProxyAction.setChecked(useSystemProxySetting());

    autoStartAction.setCheckable(true);
    autoStartAction.setChecked(autoStartSetting());
    
    // configure autostart on first run if enabled
    if (autoStartSetting()) {
        setupAutostart(true);
    }

    autoConnectAction.setCheckable(true);
    autoConnectAction.setChecked(autoConnectSetting());

    QObject::connect(&toggleAction, &QAction::triggered, [&]() {
        if (!proxyEnabled) {
            if (enableProxy()) {
                updateTray(tray, toggleAction);
            }
        } else {
            disableProxy();
            updateTray(tray, toggleAction);
        }
    });

    QObject::connect(&useSystemProxyAction, &QAction::toggled, [&](bool checked) {
        bool oldUseSystemProxy = useSystemProxySetting();
        setUseSystemProxySetting(checked);
        
        // if proxy was enabled and mode changed - reconnect
        if (proxyEnabled && oldUseSystemProxy != checked) {
            // always clear system proxy when switching mode
            disableProxy(true);
            QThread::msleep(500);
            enableProxy();
        } else if (oldUseSystemProxy && !checked) {
            // if switched from system to local, but proxy was disabled
            // still need to clear system proxy
            disableProxy(true);
        }
        
        updateTray(tray, toggleAction);
    });

    QObject::connect(&autoStartAction, &QAction::toggled, [&](bool checked) {
        setAutoStartSetting(checked);
        setupAutostart(checked);
    });

    QObject::connect(&autoConnectAction, &QAction::toggled, [&](bool checked) {
        setAutoConnectSetting(checked);
    });

    QObject::connect(&settingsAction, &QAction::triggered, [&]() {
        SettingsDialog dlg;
        if (dlg.exec() == QDialog::Accepted) {
            int oldPort = socksPort();
            QString oldHost = sshHost();
            bool oldUseSystemProxy = useSystemProxySetting();

            bool wasRunning = proxyEnabled;

            // save settings
            setSshHost(dlg.host());
            setSocksPort(dlg.port());
            setUseSystemProxySetting(dlg.useSystemProxy());
            setAutoConnectSetting(dlg.autoConnect());
            setAutoStartSetting(dlg.autoStart());
            useSystemProxyAction.setChecked(dlg.useSystemProxy());
            autoConnectAction.setChecked(dlg.autoConnect());
            autoStartAction.setChecked(dlg.autoStart());
            
            // configure autostart
            setupAutostart(dlg.autoStart());

            // reconnect on host/port change or proxy mode change
            if (wasRunning && (oldPort != dlg.port() || oldHost != dlg.host() || oldUseSystemProxy != dlg.useSystemProxy())) {
                disableProxy();
                QThread::msleep(500); // delay for proper completion
                enableProxy();
            }

            updateTray(tray, toggleAction);
        }
    });

    QObject::connect(&quitAction, &QAction::triggered, [&]() {
        if (proxyEnabled)
            disableProxy();
        app.quit();
    });

    menu.addAction(&toggleAction);
    menu.addSeparator();
    menu.addAction(&useSystemProxyAction);
    menu.addAction(&autoStartAction);
    menu.addAction(&autoConnectAction);
    menu.addSeparator();
    menu.addAction(&settingsAction);
    menu.addSeparator();
    menu.addAction(&quitAction);

    tray.setContextMenu(&menu);
    tray.show();

    // auto-connect on startup
    if (autoConnectSetting() && !proxyEnabled) {
        enableProxy();
        updateTray(tray, toggleAction);
    }

    // background status check every 2s
    QTimer timer;
    timer.setInterval(2000);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        bool now = isProxyRunning();
        if (now != proxyEnabled) {
            // state changed externally - only update UI
            proxyEnabled = now;
            updateTray(tray, toggleAction);
            
            tray.showMessage("SSH Proxy",
                             proxyEnabled ? "Detected external start" : "Detected stop",
                             QSystemTrayIcon::Information, 2000);
        }
    });
    timer.start();

    return app.exec();
}
//+-----------------------------------------------------------------+
