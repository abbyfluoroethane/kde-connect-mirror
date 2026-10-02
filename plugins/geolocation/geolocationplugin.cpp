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

K_PLUGIN_CLASS_WITH_JSON(GeolocationPlugin, "kdeconnect_geolocation.json");

GeolocationPlugin::GeolocationPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
    Daemon *d = Daemon::instance();
    connect(d, &Daemon::activeGpsProviderChanged, this, &GeolocationPlugin::providerChanged);
    connect(this, &GeolocationPlugin::onDataReceived, d, &Daemon::onNmeaDataReceived);
}

void GeolocationPlugin::providerChanged(const QString &deviceId)
{
    sendLocationRequest(deviceId == device()->id());
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

GeolocationPlugin::~GeolocationPlugin() = default;

void GeolocationPlugin::receivePacket(const NetworkPacket &np)
{
    auto a = np.get<QString>(QStringLiteral("nmea"));
    Q_EMIT onDataReceived(a);
}

QString GeolocationPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/geolocation").arg(device()->id());
}

#include "moc_geolocationplugin.cpp"
#include "geolocationplugin.moc"
