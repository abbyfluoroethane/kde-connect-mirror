/*
 * SPDX-FileCopyrightText: 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2020 Piyush Aggarwal <piyushaggarwal002@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "deviceindicator.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QStandardPaths>

#include "interfaces/dbusinterfaces.h"
#include "systray_actions/volumedialog.h"

#include <dbushelper.h>
#include <dbushelpers.h>
#include <systray_actions.h>

#include "plugins/mobilevolume/mobilevolumeplugin.h"

DeviceIndicator::DeviceIndicator(DeviceDbusInterface *device)
    : QMenu(device->name(), nullptr)
    , m_device(device)
    , m_remoteCommandsInterface(new RemoteCommandsDbusInterface(m_device->id()))
{
    setIcon(QIcon::fromTheme(device->iconName()));

    connect(device, &DeviceDbusInterface::nameChanged, this, &DeviceIndicator::setText);

    // Battery status
    auto battery = new BatteryAction(device);
    addAction(battery);
    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_battery")),
        [battery](bool error, bool available) {
            battery->setVisible(available && !error);
            battery->setDisabled(true);
        },
        this);

    auto connectivity = new ConnectivityAction(device);
    addAction(connectivity);
    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_connectivity_report")),
        [connectivity](bool error, bool available) {
            connectivity->setVisible(available && !error);
            connectivity->setDisabled(true);
        },
        this);

    this->addSeparator();

    // Browse device filesystem
    auto browse = addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18n("Browse device"));
    connect(browse, &QAction::triggered, device, [device]() {
        SftpDbusInterface *sftpIface = new SftpDbusInterface(device->id(), device);
        sftpIface->startBrowsing();
        sftpIface->deleteLater();
    });
    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_sftp")),
        [browse](bool error, bool available) {
            browse->setVisible(available && !error);
        },
        this);

    // Clipboard
    auto clipboard = addAction(QIcon::fromTheme(QStringLiteral("klipper")), i18n("Send clipboard"));
    connect(clipboard, &QAction::triggered, device, [device]() {
        ClipboardDbusInterface *clipboardIface = new ClipboardDbusInterface(device->id(), device);
        clipboardIface->sendClipboard();
        clipboardIface->deleteLater();
    });
    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_clipboard")),
        [clipboard](bool error, bool available) {
            clipboard->setVisible(available && !error);
        },
        this);

    // Find device
    auto findDevice = addAction(QIcon::fromTheme(QStringLiteral("irc-voice")), i18nc("@action:inmenu play bell sound", "Ring device"));
    connect(findDevice, &QAction::triggered, device, [device]() {
        FindMyPhoneDeviceDbusInterface *iface = new FindMyPhoneDeviceDbusInterface(device->id(), device);
        iface->ring();
        iface->deleteLater();
    });
    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_findmyphone")),
        [findDevice](bool error, bool available) {
            findDevice->setVisible(available && !error);
        },
        this);

    // Send file
    auto sendFile = addAction(QIcon::fromTheme(QStringLiteral("document-share")), i18n("Send files"));
    connect(sendFile, &QAction::triggered, device, [device, this]() {
        const QList<QUrl> urls = QFileDialog::getOpenFileUrls(this);
        if (!urls.isEmpty()) {
            QStringList urlsAsQStringList;
            for (const QUrl &url : urls)
                urlsAsQStringList.append(url.toString());
            ShareDbusInterface *iface = new ShareDbusInterface(device->id(), device);
            iface->shareUrls(urlsAsQStringList);
            iface->deleteLater();
        }
    });
    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_share")),
        [sendFile](bool error, bool available) {
            sendFile->setVisible(available && !error);
        },
        this);

    // SMS Messages
    const QString kdeconnectsmsExecutable = QStandardPaths::findExecutable(QStringLiteral("kdeconnect-sms"), {QCoreApplication::applicationDirPath()});
    if (!kdeconnectsmsExecutable.isEmpty()) {
        auto smsapp = addAction(QIcon::fromTheme(QStringLiteral("message-new")), i18n("SMS Messages…"));
        QObject::connect(smsapp, &QAction::triggered, device, [device, kdeconnectsmsExecutable]() {
            QProcess::startDetached(kdeconnectsmsExecutable, {QStringLiteral("--device"), device->id()});
        });
        setWhenAvailable(
            device->hasPlugin(QStringLiteral("kdeconnect_sms")),
            [smsapp](bool error, bool available) {
                smsapp->setVisible(available && !error);
            },
            this);
    }

    // Run command
    QMenu *remoteCommandsMenu = new QMenu(i18n("Run command"), this);
    QAction *menuAction = remoteCommandsMenu->menuAction();
    QAction *addCommandAction = remoteCommandsMenu->addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Add commands"));
    connect(addCommandAction, &QAction::triggered, m_remoteCommandsInterface, &RemoteCommandsDbusInterface::editCommands);

    addAction(menuAction);
    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_remotecommands")),
        [this, remoteCommandsMenu, menuAction](bool error, bool available) {
            menuAction->setVisible(available && !error);

            if (!available || error) {
                return;
            }

            const auto cmds = QJsonDocument::fromJson(m_remoteCommandsInterface->commands()).object();

            for (auto it = cmds.constBegin(), itEnd = cmds.constEnd(); it != itEnd; ++it) {
                const QJsonObject cont = it->toObject();
                QString key = it.key();
                QAction *action = remoteCommandsMenu->addAction(cont.value(QStringLiteral("name")).toString());
                connect(action, &QAction::triggered, [this, key] {
                    m_remoteCommandsInterface->triggerCommand(key);
                });
            }
        },
        this);

    // Open mobile volume control
    auto volumeControl = addAction(QIcon::fromTheme(QStringLiteral("audio-volume-high")), i18n("Volume Control"));
    connect(volumeControl, &QAction::triggered, this, &DeviceIndicator::showVolumeDialog);

    setWhenAvailable(
        device->hasPlugin(QStringLiteral("kdeconnect_mobilevolume")),
        [volumeControl](bool error, bool available) {
            volumeControl->setVisible(available && !error);
        },
        this);
}

void DeviceIndicator::showVolumeDialog()
{
    qDebug() << "=== DEBUG: Checking MobileVolume Plugin ===";
    qDebug() << "Device name:" << m_device->name();
    qDebug() << "Device ID:" << m_device->id();

    // Check all loaded plugins
    QStringList plugins = m_device->loadedPlugins();
    qDebug() << "All loaded plugins:" << plugins;

    // Check if mobilevolume is available
    bool hasPlugin = m_device->hasPlugin(QStringLiteral("kdeconnect_mobilevolume"));
    qDebug() << "Has mobilevolume plugin:" << hasPlugin;

    if (!hasPlugin) {
        qWarning() << "MobileVolume plugin not found! Available plugins:" << plugins;
        return;
    }

    // Check if the DBus service is available
    QString dbusPath = QStringLiteral("/modules/kdeconnect/devices/") + m_device->id() + QStringLiteral("/mobilevolume");
    QDBusConnection sessionBus = QDBusConnection::sessionBus();

    if (!sessionBus.interface()->isServiceRegistered(QStringLiteral("org.kde.kdeconnect"))) {
        qWarning() << "KDE Connect DBus service not available";
        return;
    }

    // Create or show the volume dialog
    if (!m_volumeDialog) {
        m_volumeDialog = new VolumeDialog(m_device->id(), nullptr);
        // Connect destroyed signal to reset pointer
        connect(m_volumeDialog, &QObject::destroyed, this, [this]() {
            m_volumeDialog = nullptr;
        });
    }

    m_volumeDialog->show();
    m_volumeDialog->raise();
    m_volumeDialog->activateWindow();
}

#include "moc_deviceindicator.cpp"
