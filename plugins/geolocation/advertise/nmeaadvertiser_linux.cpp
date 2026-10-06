/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "generated/systeminterfaces/avahientrygroup.h"
#include "generated/systeminterfaces/avahiserver.h"
#include "nmeaadvertiser.h"
#include "plugin_geolocation_debug.h"

struct NmeaAdvertiserPrivate {
    std::unique_ptr<OrgFreedesktopAvahiServer2Interface> avahiServerInterface;
    std::unique_ptr<OrgFreedesktopAvahiEntryGroupInterface> avahiEntryGroupInterface;
};

NmeaAdvertiser::NmeaAdvertiser(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<NmeaAdvertiserPrivate>())
{
    d->avahiServerInterface =
        std::make_unique<OrgFreedesktopAvahiServer2Interface>(QStringLiteral("org.freedesktop.Avahi"), QStringLiteral("/"), QDBusConnection::systemBus(), this);
}

NmeaAdvertiser::~NmeaAdvertiser()
{
    stopAdvertising();
}

bool NmeaAdvertiser::startAdvertising()
{
    if (d->avahiEntryGroupInterface) {
        return true;
    }
    auto reply = d->avahiServerInterface->EntryGroupNew();
    reply.waitForFinished();

    if (reply.isError()) {
        qCWarning(KDECONNECT_PLUGIN_GEOLOCATION) << "Could not reach avahi:" << reply.error().message();
        return false;
    }

    const QString groupPath = reply.value().path();

    d->avahiEntryGroupInterface =
        std::make_unique<OrgFreedesktopAvahiEntryGroupInterface>(QStringLiteral("org.freedesktop.Avahi"), groupPath, QDBusConnection::systemBus(), this);

    qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Advertising _nmea-0183._tcp with Avahi.";
    d->avahiEntryGroupInterface->AddService(-1,
                                            -1,
                                            0,
                                            QStringLiteral("KDE Connect NMEA"),
                                            QStringLiteral("_nmea-0183._tcp"),
                                            QStringLiteral(""),
                                            QStringLiteral(""),
                                            10110,
                                            QList<QByteArray>());

    d->avahiEntryGroupInterface->Commit();
    return true;
}

void NmeaAdvertiser::stopAdvertising()
{
    if (d->avahiEntryGroupInterface) {
        auto reply = d->avahiEntryGroupInterface->Free();
        reply.waitForFinished();
        d->avahiEntryGroupInterface.reset();
    }
}

#include "moc_nmeaadvertiser.cpp"