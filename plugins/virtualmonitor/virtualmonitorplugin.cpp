/**
 * SPDX-FileCopyrightText: 2021 Aleix Pol i Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "virtualmonitorplugin.h"

#include <KPluginFactory>

#include <QDesktopServices>
#include <QGuiApplication>
#include <QJsonArray>
#include <QProcess>
#include <QScreen>
#include "plugin_virtualmonitor_debug.h"

K_PLUGIN_CLASS_WITH_JSON(VirtualMonitorPlugin, "kdeconnect_virtualmonitor.json")
#define QS QLatin1String

VirtualMonitorPlugin::VirtualMonitorPlugin(QObject* parent, const QVariantList& args)
    : KdeConnectPlugin(parent, args)
{
}

VirtualMonitorPlugin::~VirtualMonitorPlugin()
{
    stop();
}

void VirtualMonitorPlugin::stop()
{
    if (!m_process)
        return;

    m_process->terminate();
    if (!m_process->waitForFinished()) {
        m_process->kill();
        m_process->waitForFinished();
    }
    delete m_process;
    m_process = nullptr;
}

void VirtualMonitorPlugin::connected()
{
    auto resolution = QGuiApplication::primaryScreen()->size();
    QString resolutionString = QString::number(resolution.width()) + QLatin1Char('x') + QString::number(resolution.height());
    NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR, { { QS("resolutions"), QJsonArray { resolutionString } } });
    sendPacket(np);
}

bool VirtualMonitorPlugin::receivePacket(const NetworkPacket& received)
{
    if (received.type() == PACKET_TYPE_VIRTUALMONITOR_REQUEST && received.has(QS("start"))) {
        QUrl url;
        url.setScheme(QS("vnc"));
        url.setUserName(QS("user"));
        url.setPassword(received.get<QString>(QS("password")));
        url.setHost(device()->getLocalIpAddress().toString());
        if (!QDesktopServices::openUrl(url)) {
            qCWarning(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Failed to open" << url.toDisplayString();
            NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR, { { QS("failed"), 0 } });
            sendPacket(np);
        }
    } else if (received.type() == PACKET_TYPE_VIRTUALMONITOR) {
        if (received.has(QS("resolutions"))) {
            m_remoteResolution = received.get<QStringList>(QS("resolutions")).constFirst();
        }
        if (received.has(QS("failed"))) {
            stop();
        }
    }
    return true;
}

QString VirtualMonitorPlugin::dbusPath() const
{
    return QS("/modules/kdeconnect/devices/") + device()->id() + QS("/virtualmonitor");
}

bool VirtualMonitorPlugin::requestVirtualMonitor()
{
    stop();
    if (m_remoteResolution.isEmpty()) {
        qCWarning(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Cannot start a request without a resolution";
        return false;
    }

    QUuid uuid;
    const QString port = QS("5900");

    m_process = new QProcess(this);
    m_process->setProgram(QS("krfb-virtualmonitor"));
    m_process->setArguments({QS("--name"), device()->name(), QS("--resolution"), m_remoteResolution, QS("--password"), uuid.toString(), QS("--port"), port});
    m_process->start();

    if (!m_process->waitForStarted()) {
        qCWarning(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Failed to start krfb" << m_process->error() << m_process->errorString();
        return false;
    }
    NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR, {
        { QS("start"), 1 },
        { QS("password"), uuid.toString() },
        { QS("port"), port },
    });
    sendPacket(np);
    return true;
}

#include "virtualmonitorplugin.moc"
