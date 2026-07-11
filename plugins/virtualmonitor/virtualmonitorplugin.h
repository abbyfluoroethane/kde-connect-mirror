/**
 * SPDX-FileCopyrightText: 2021 Aleix Pol i Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2024 Fabian Arndt <fabian.arndt@root-core.net>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "plugin_virtualmonitor_debug.h"
#include <QByteArray>
#include <QJsonObject>
#include <QPointer>
#include <core/kdeconnectplugin.h>

#include <memory>
#include <optional>

#define PACKET_TYPE_VIRTUALMONITOR QStringLiteral("kdeconnect.virtualmonitor")
#define PACKET_TYPE_VIRTUALMONITOR_REQUEST QStringLiteral("kdeconnect.virtualmonitor.request")

class KNotification;
class QProcess;
class QTemporaryDir;

class VirtualMonitorPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.virtualmonitor")
    Q_PROPERTY(QString lastError READ lastError)
    Q_PROPERTY(bool isVirtualMonitorAvailable READ isVirtualMonitorAvailable CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    VirtualMonitorPlugin(QObject *parent, const QVariantList &args);
    ~VirtualMonitorPlugin() override;

    Q_SCRIPTABLE bool requestVirtualMonitor();

    /// stops the active session
    Q_SCRIPTABLE void stop();

    /// Revokes persistent screen-viewing and remote-control permission for this device.
    Q_SCRIPTABLE void revokeRemoteDesktopPermission();

    QString lastError() const
    {
        return m_lastError;
    }

    void connected() override;
    QString dbusPath() const override;
    void receivePacket(const NetworkPacket &np) override;

    // The remote device has a RDP client installed
    bool hasRemoteClient() const
    {
        return m_capabilitiesRemote.rdpClient;
    }

    // krdpserver is installed on the local device and can export a desktop
    bool isVirtualMonitorAvailable() const
    {
        return m_capabilitiesLocal.virtualMonitor;
    }

    /// Returns true if there's a running virtual monitor
    bool active() const;

Q_SIGNALS:
    Q_SCRIPTABLE void activeChanged();

private:
    enum class SessionMode {
        VirtualMonitor,
        Monitor,
        Workspace,
    };

    struct SessionRequest {
        QString sessionId;
        SessionMode mode = SessionMode::VirtualMonitor;
        QString resolution;
        double scale = 1.0;
        int monitorIndex = 0;
        int quality = 70;
    };

    bool checkRdpClient() const;
    bool checkDefaultSchemeHandler(const QString &scheme) const;
    bool remoteDesktopPermissionGranted() const;
    QString pairedDeviceCertificateFingerprint() const;
    void requestRemoteDesktopPermission(const SessionRequest &request);
    void allowRemoteDesktopPermission(const QString &sessionId);
    void rejectRemoteDesktopPermission(const QString &sessionId, const QString &message);
    void clearRemoteDesktopPermissionPrompt();
    bool startRdpSession(const SessionRequest &request);
    void stopSession(bool notifyRemote);
    void handleSessionRequest(const NetworkPacket &np);
    void openRdpClient(const NetworkPacket &np);
    void probeServerReady(quint64 generation, int attemptsRemaining);
    void sendConnectionInfo();
    void sendStatus(const QString &status, const QString &message = {});
    void sendStatusForSession(const QString &sessionId, const QString &status, const QString &message = {});
    bool prepareTlsCredentials();
    quint16 findAvailablePort() const;

    struct Capabilities {
        bool virtualMonitor = false;
        bool remoteDesktop = false;
        bool rdpClient = false;
    };

    Capabilities m_capabilitiesLocal;
    Capabilities m_capabilitiesRemote;

    QProcess *m_process = nullptr;
    QPointer<KNotification> m_permissionNotification;
    std::unique_ptr<QTemporaryDir> m_tlsDirectory;
    std::optional<SessionRequest> m_pendingPermissionRequest;
    QJsonObject m_remoteResolution;
    SessionRequest m_currentRequest;
    QString m_username;
    QString m_password;
    QString m_certificateFingerprint;
    quint16 m_port = 0;
    quint64 m_sessionGeneration = 0;
    bool m_serverReady = false;
    bool m_clientConnected = false;
    bool m_stopping = false;
    bool m_deviceLifecycleConnected = false;
    QString m_lastError;
    QByteArray m_serverErrors;
};
