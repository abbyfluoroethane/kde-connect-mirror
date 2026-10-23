/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "geolocationworker.h"

#include <QNetworkInterface>

#include "geolocationplugin.h"

#include <QTcpServer>
#include <QTcpSocket>

#include <core/daemon.h>
#include <core/device.h>

#include "plugin_geolocation_debug.h"

GeolocationWorker *GeolocationWorker::instance()
{
    static GeolocationWorker instance;
    return &instance;
}

GeolocationWorker::GeolocationWorker(QObject *parent)
    : QObject(parent)
    , m_localServer(new QTcpServer(this))
    , m_nmeaAdvertiser(this)
{
    connect(m_localServer, &QTcpServer::newConnection, this, &GeolocationWorker::slotNewConnection);

    Daemon *d = Daemon::instance();
    connect(d, &Daemon::gpsAllowRemoteChanged, this, [this](bool allow) {
        if (!allow) {
            dropConsumers(true);
        }
    });
    connect(d, &Daemon::preferredGpsProviderChanged, this, &GeolocationWorker::chooseGpsProvider);
    connect(this, &GeolocationWorker::pluginsUpdated, this, &GeolocationWorker::chooseGpsProvider);
    connect(this, &GeolocationWorker::consumersUpdated, this, &GeolocationWorker::chooseGpsProvider);
}

GeolocationWorker::~GeolocationWorker()
{
    qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Destroying NMEA socket";
    m_nmeaAdvertiser.stopAdvertising();
    m_localServer->close();
}

void GeolocationWorker::updateServerState()
{
    const bool enabled = !m_plugins.isEmpty();
    if (enabled == m_localServer->isListening()) {
        return;
    }

    if (enabled) {
        if (m_localServer->listen(QHostAddress::Any, 10110)) {
            m_nmeaAdvertiser.startAdvertising();
        } else {
            qCWarning(KDECONNECT_PLUGIN_GEOLOCATION) << "Failed to bind to port 10110:" << m_localServer->errorString();
        }
    } else {
        qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Stopping NMEA server";
        m_nmeaAdvertiser.stopAdvertising();
        m_localServer->close();
        dropConsumers(false);
    }
}

void GeolocationWorker::registerPlugin(GeolocationPlugin *plugin)
{
    if (!plugin || m_plugins.contains(plugin)) {
        return;
    }
    m_plugins.append(plugin);
    updateServerState();
    Q_EMIT pluginsUpdated();
}

void GeolocationWorker::unregisterPlugin(GeolocationPlugin *plugin)
{
    if (!plugin) {
        return;
    }
    m_plugins.removeOne(plugin);
    if (plugin->isActive()) {
        m_activeProviderId.clear();
    }
    updateServerState();
    Q_EMIT pluginsUpdated();
}

void GeolocationWorker::chooseGpsProvider()
{
    QString newActiveId;

    if (!m_consumers.isEmpty()) {
        const QString preferredProvider = Daemon::instance()->preferredGpsProvider();
        QHash<QString, Device *> candidates;

        for (GeolocationPlugin *plugin : std::as_const(m_plugins)) {
            const Device *device = plugin->device();
            if (device && device->isReachable() && device->isPaired()) {
                candidates.emplace(device->id(), device);
            }
        }

        if (candidates.contains(preferredProvider)) {
            newActiveId = preferredProvider;
            qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Using Geolocation data from preferred provider:" << candidates[preferredProvider]->name();
        } else if (!candidates.isEmpty()) {
            newActiveId = candidates.constBegin().key();
            qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Preferred Geolocation device not accessible, using first available device:"
                                                   << candidates[newActiveId]->name();
        } else {
            qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "No Geolocation providers available.";
        }
    }

    if (m_activeProviderId != newActiveId) {
        m_activeProviderId = newActiveId;
        for (GeolocationPlugin *plugin : std::as_const(m_plugins)) {
            if (plugin->device()) {
                plugin->setActive(plugin->device()->id() == m_activeProviderId);
            }
        }
    }
}

static bool isLocalConnection(const QHostAddress &address)
{
    if (address.isLoopback()) {
        return true;
    }

    const QList<QHostAddress> localAddresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &localAddress : localAddresses) {
        if (address.isEqual(localAddress, QHostAddress::TolerantConversion)) {
            return true;
        }
    }

    return false;
}

void GeolocationWorker::slotNewConnection()
{
    bool updated = false;
    while (m_localServer->hasPendingConnections()) {
        QTcpSocket *consumer = m_localServer->nextPendingConnection();
        if (!consumer) {
            continue;
        }

        if (!Daemon::instance()->gpsAllowRemote() && !isLocalConnection(consumer->peerAddress())) {
            qCWarning(KDECONNECT_PLUGIN_GEOLOCATION) << "Dropping connection from non-local address: " << consumer->peerAddress().toString();
            consumer->abort();
            consumer->deleteLater();
            continue;
        }

        m_consumers.append(consumer);
        updated = true;
        connect(consumer, &QTcpSocket::disconnected, this, [this, consumer]() {
            slotDisconnected(consumer);
        });
        qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Consumer connected to NMEA socket. Consumers:" << m_consumers.length();
    }
    if (updated) {
        Q_EMIT consumersUpdated();
    }
}

void GeolocationWorker::slotDisconnected(QTcpSocket *consumer)
{
    if (consumer) {
        consumer->disconnect(this);
        consumer->deleteLater();
        m_consumers.removeOne(consumer);
        Q_EMIT consumersUpdated();
        qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Consumer closed connection to NMEA socket. Consumers:" << m_consumers.length();
    }
}

void GeolocationWorker::dropConsumers(bool onlyRemote)
{
    const auto consumers = m_consumers;
    for (QTcpSocket *consumer : consumers) {
        if (onlyRemote && isLocalConnection(consumer->peerAddress())) {
            continue;
        }
        consumer->disconnect(this);
        consumer->abort();
        consumer->deleteLater();
        m_consumers.removeOne(consumer);
    }
    if (consumers.length() != m_consumers.length()) {
        Q_EMIT consumersUpdated();
    }
}

void GeolocationWorker::forwardNmeaPacket(const QString &deviceId, const QString &data)
{
    if (deviceId != m_activeProviderId) {
        return;
    }

    const QByteArray bytes = data.toLatin1();
    for (QTcpSocket *consumer : std::as_const(m_consumers)) {
        if (consumer && consumer->isWritable()) {
            consumer->write(bytes);
        }
    }
}
