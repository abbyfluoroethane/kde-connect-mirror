/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "geolocationplugin.h"

#include <KPluginFactory>

#include <QDBusConnection>
#include <QDebug>
#include <QThread>

#include <core/daemon.h>
#include <core/device.h>
#include <sys/socket.h>
#include "socketworker.h"
#include "plugin_geolocation_debug.h"

K_PLUGIN_CLASS_WITH_JSON(GeolocationPlugin, "kdeconnect_geolocation.json");

GeolocationPlugin::GeolocationPlugin(QObject *parent, const QVariantList &args):
    KdeConnectPlugin(parent, args)
{
    m_socketWorker = new SocketWorker();
    m_thread = new QThread(this);
    m_socketWorker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_socketWorker, &SocketWorker::process);
    connect(m_thread, &QThread::finished, m_socketWorker, &QObject::deleteLater);
    connect(m_socketWorker, &SocketWorker::startedListening, this, &GeolocationPlugin::consumerConnected);
    connect(m_socketWorker, &SocketWorker::stoppedListening, this, &GeolocationPlugin::consumerDisconnected);
    connect(this, SIGNAL(onDataReceived(QString)), m_socketWorker, SLOT(sendToCurrentClient(QString)));
    m_thread->start();
}

void GeolocationPlugin::consumerConnected() const {
    sendLocationRequest(true);
}

void GeolocationPlugin::consumerDisconnected() const {
    sendLocationRequest(false);
}

void GeolocationPlugin::sendLocationRequest(bool enable) const {
    NetworkPacket np(PACKET_TYPE_LOCATION_REQUEST);
    np.set(QStringLiteral("enable"), enable);
    sendPacket(np);
}

GeolocationPlugin::~GeolocationPlugin() {
    if (m_thread != nullptr) {
        m_thread->quit();
        m_thread->wait();
    }
}

void GeolocationPlugin::receivePacket(const NetworkPacket &np)
{
    auto a = np.get<QString>(QStringLiteral("nmea"));
    if (m_socketWorker != nullptr)
        onDataReceived(a);
}

QString GeolocationPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/geolocation").arg(device()->id());
}

#include "moc_geolocationplugin.cpp"
#include "geolocationplugin.moc"
