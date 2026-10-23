/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "geolocationplugin.h"
#include "geolocationworker.h"

#include <KPluginFactory>
#include <core/device.h>
#include <core/kdeconnectpluginconfig.h>

K_PLUGIN_CLASS_WITH_JSON(GeolocationPlugin, "kdeconnect_geolocation.json")

GeolocationPlugin::GeolocationPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_device(device())
{
    connect(config(), &KdeConnectPluginConfig::configChanged, this, &GeolocationPlugin::configChanged);
    m_allowRemote = config()->getBool(QStringLiteral("allowRemote"), false);
    GeolocationWorker::instance()->registerPlugin(this);
}

GeolocationPlugin::~GeolocationPlugin()
{
    GeolocationWorker::instance()->unregisterPlugin(this);
    if (m_active && m_device && m_device->isPaired() && m_device->isReachable()) {
        sendLocationRequest(false);
    }
}

void GeolocationPlugin::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        sendLocationRequest(m_active);
    }
}

bool GeolocationPlugin::isActive() const
{
    return m_active;
}

bool GeolocationPlugin::allowRemote() const
{
    return m_allowRemote;
}

void GeolocationPlugin::configChanged()
{
    m_allowRemote = config()->getBool(QStringLiteral("allowRemote"), false);
    GeolocationWorker::instance()->updateRemoteAccess();
}

void GeolocationPlugin::sendLocationRequest(bool enable) const
{
    NetworkPacket np(PACKET_TYPE_LOCATION_REQUEST);
    np.set(QStringLiteral("enable"), enable);
    sendPacket(np);
}

void GeolocationPlugin::receivePacket(const NetworkPacket &np)
{
    const QString nmea = np.get<QString>(QStringLiteral("nmea"));
    GeolocationWorker::instance()->forwardNmeaPacket(device()->id(), nmea);
}

QString GeolocationPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/geolocation").arg(device()->id());
}

#include "geolocationplugin.moc"
#include "moc_geolocationplugin.cpp"
