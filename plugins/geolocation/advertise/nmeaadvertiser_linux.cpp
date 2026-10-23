/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include "generated/systeminterfaces/avahientrygroup.h"
#include "generated/systeminterfaces/avahiserver.h"
#include "nmeaadvertiser.h"
#include "plugin_geolocation_debug.h"

struct NmeaAdvertiserPrivate {
    std::unique_ptr<OrgFreedesktopAvahiServer2Interface> avahiServerInterface;
    std::unique_ptr<OrgFreedesktopAvahiEntryGroupInterface> avahiEntryGroupInterface;
    QDBusPendingCallWatcher *pendingEntryGroup = nullptr;
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

void NmeaAdvertiser::startAdvertising()
{
    if (d->avahiEntryGroupInterface || d->pendingEntryGroup) {
        return;
    }

    d->pendingEntryGroup = new QDBusPendingCallWatcher(d->avahiServerInterface->EntryGroupNew(), this);
    connect(d->pendingEntryGroup, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        d->pendingEntryGroup = nullptr;

        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qCWarning(KDECONNECT_PLUGIN_GEOLOCATION) << "Could not reach avahi:" << reply.error().message();
            return;
        }

        d->avahiEntryGroupInterface = std::make_unique<OrgFreedesktopAvahiEntryGroupInterface>(QStringLiteral("org.freedesktop.Avahi"),
                                                                                               reply.value().path(),
                                                                                               QDBusConnection::systemBus(),
                                                                                               this);

        qCDebug(KDECONNECT_PLUGIN_GEOLOCATION) << "Advertising _nmea-0183._tcp with Avahi.";

        d->avahiEntryGroupInterface
            ->AddService(-1, -1, 0, QStringLiteral("KDE Connect NMEA"), QStringLiteral("_nmea-0183._tcp"), QString(), QString(), 10110, QList<QByteArray>());
        d->avahiEntryGroupInterface->Commit();
    });
}

void NmeaAdvertiser::stopAdvertising()
{
    // Dropping the watcher cancels the callback if EntryGroupNew is still pending
    delete d->pendingEntryGroup;
    d->pendingEntryGroup = nullptr;

    if (d->avahiEntryGroupInterface) {
        d->avahiEntryGroupInterface->Free();
        d->avahiEntryGroupInterface.reset();
    }
}

#include "moc_nmeaadvertiser.cpp"
