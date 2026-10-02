/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "geolocationworker.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <dbushelper.h>

#include "daemon.h"
#include "generated/systeminterfaces/avahientrygroup.h"
#include "generated/systeminterfaces/avahiserver.h"
#include "kdeconnect_debug.h"

GeolocationWorker::GeolocationWorker(Daemon *parent)
    : QObject(parent)
    , m_daemon(parent)
    , m_localServer(new QTcpServer(this))
    , m_avahiServerInterface(std::make_unique<OrgFreedesktopAvahiServer2Interface>(QStringLiteral("org.freedesktop.Avahi"),
                                                                                   QStringLiteral("/"),
                                                                                   QDBusConnection::systemBus(),
                                                                                   this))
{
    if (m_localServer->listen(QHostAddress::Any, 10110)) {
        m_avahiEntryGroupInterface = advertise();
    } else
    {
        qCWarning(KDECONNECT_CORE) << "Failed to bind to port 10110: " << m_localServer->errorString();
    }
    connect(m_localServer, &QTcpServer::newConnection, this, &GeolocationWorker::slotNewConnection);

    connect(this, &GeolocationWorker::consumersChanged, this, &GeolocationWorker::chooseGpsProvider);
    connect(m_daemon, &Daemon::deviceListChanged, this, &GeolocationWorker::chooseGpsProvider);
    connect(m_daemon, &Daemon::preferredGpsProviderChanged, this, &GeolocationWorker::chooseGpsProvider);
}

std::unique_ptr<OrgFreedesktopAvahiEntryGroupInterface> GeolocationWorker::advertise() {

    auto reply = m_avahiServerInterface->EntryGroupNew();
    reply.waitForFinished();

    if (reply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not reach avahi.";
        return nullptr;
    }

    QString groupPath = reply.value().path();

    auto entryGroup = std::make_unique<OrgFreedesktopAvahiEntryGroupInterface>(
        QStringLiteral("org.freedesktop.Avahi"),
        groupPath,
        QDBusConnection::systemBus(),
        this
    );

    qCInfo(KDECONNECT_CORE) << "Advertising _nmea-0183._tcp with Avahi.";
    entryGroup->AddService(
        -1,
        -1,
        0,
        QStringLiteral("KDE Connect NMEA"),
        QStringLiteral("_nmea-0183._tcp"),
        QStringLiteral(""),
        QStringLiteral(""),
        10110,
        QList<QByteArray>()
    );

    entryGroup->Commit();
    return entryGroup;
}

GeolocationWorker::~GeolocationWorker() {
    qCInfo(KDECONNECT_CORE) << "Destroying NMEA socket";
    if (m_avahiEntryGroupInterface != nullptr) {
        auto reply = m_avahiEntryGroupInterface->Free();
        reply.waitForFinished();
    }
    m_localServer->close();
}

void GeolocationWorker::chooseGpsProvider()
{
    if (m_consumers.isEmpty()) {
        Q_EMIT m_daemon->activeGpsProviderChanged(QStringLiteral(""));
        return;
    }
    QString preferredProvider = m_daemon->preferredGpsProvider();
    QStringList candidates;
    for (const QString &deviceId : m_daemon->devices()) {
        Device *device = m_daemon->getDevice(deviceId);
        if (device->isReachable() && device->hasPlugin(QStringLiteral("kdeconnect_geolocation"))) {
            candidates.append(deviceId);
        }
    }
    QString ret;
    if (candidates.contains(preferredProvider)) {
        qCDebug(KDECONNECT_CORE) << "Using Geolocation data from preferred provider: " << m_daemon->getDevice(preferredProvider)->name();
        ret = preferredProvider;
    } else if (!candidates.isEmpty()) {
        ret = candidates.first();
        qCDebug(KDECONNECT_CORE) << "Preferred Geolocation device not accessible, using first available device: " << m_daemon->getDevice(ret)->name();
    } else {
        qCDebug(KDECONNECT_CORE) << "No Geolocation providers available.";
        ret = QStringLiteral("");
    }
    Q_EMIT m_daemon->activeGpsProviderChanged(ret);
}

void GeolocationWorker::slotNewConnection() {
    while (m_localServer->hasPendingConnections())
    {
        QTcpSocket* consumer = m_localServer->nextPendingConnection();
        m_consumers.append(consumer);
        Q_EMIT consumersChanged();
        connect(consumer, &QTcpSocket::disconnected, this, [this, consumer] ()
        {
            slotDisconnected(consumer);
        });
        qCInfo(KDECONNECT_CORE) << "Consumer connected to NMEA socket. "
                                   "Consumers: "
                                << m_consumers.length();
    }
}

void GeolocationWorker::forwardNmeaPacket(const QString &data)
{
    for (QTcpSocket* consumer : m_consumers) {
        if (consumer && consumer->isWritable()) {
            consumer->write(data.toLatin1());
        }
    }
}

void GeolocationWorker::slotDisconnected(QTcpSocket* consumer) {
    if (consumer != nullptr) {
        consumer->disconnect(this);
        consumer->deleteLater();
        m_consumers.removeOne(consumer);
        Q_EMIT consumersChanged();
        qCInfo(KDECONNECT_CORE) << "Consumer closed connection to NMEA socket. Consumers: " << m_consumers.length();
    }
}
