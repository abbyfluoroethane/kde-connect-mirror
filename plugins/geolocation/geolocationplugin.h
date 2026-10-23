/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QPointer>

#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_LOCATION_REQUEST QStringLiteral("kdeconnect.geolocation.request")
#define PACKET_TYPE_LOCATION_NMEA QStringLiteral("kdeconnect.geolocation.nmea")

class GeolocationPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.geolocation")

public:
    using KdeConnectPlugin::KdeConnectPlugin;
    explicit GeolocationPlugin(QObject *parent, const QVariantList &args);
    ~GeolocationPlugin() override;

    void receivePacket(const NetworkPacket &np) override;
    QString dbusPath() const override;

    void setActive(bool active);
    bool isActive() const;

private:
    void sendLocationRequest(bool enable) const;
    bool m_active = false;
    // Null once the device starts being destroyed, e.g. on daemon shutdown
    QPointer<const Device> m_device;
};
