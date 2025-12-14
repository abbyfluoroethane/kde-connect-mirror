/**
 * SPDX-FileCopyrightText: 2025 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "avahidiscovery.h"

#include "core_debug.h"
#include "kdeconnectconfig.h"
#include "lanlinkprovider.h"

const QString kServiceType = QStringLiteral("_kdeconnect._udp");

AvahiDiscovery::AvahiDiscovery(LanLinkProvider *lanLinkProvider)
    : m_avahiServerInterface(QStringLiteral("org.freedesktop.Avahi"), QStringLiteral("/"), QDBusConnection::systemBus())
    , m_serviceBrowserInterface(QStringLiteral("org.freedesktop.Avahi"), QStringLiteral("/"), QDBusConnection::systemBus())
    , m_entryGroupInterface(nullptr)
{
    // Discovery callback
    connect(&m_serviceBrowserInterface,
            &OrgFreedesktopAvahiServiceBrowserInterface::ItemNew,
            this,
            [this, lanLinkProvider](int interface, int protocol, const QString &name, const QString &type, const QString &domain) {
                if (KdeConnectConfig::instance().deviceId() == name) {
                    qCDebug(KDECONNECT_CORE) << "Discovered myself, ignoring";
                    return;
                }

                // interface, protocol, name, type, domain, host, aprotocol, address, port, txt, flags
                QDBusPendingReply<int, int, QString, QString, QString, QString, int, QString, ushort, QStringList, uint> service =
                    m_avahiServerInterface.ResolveService(interface, protocol, name, type, domain, -1, 0);
                service.waitForFinished();
                if (service.isError()) {
                    qCWarning(KDECONNECT_CORE) << "Could not resolve Avahi service for" << name << ":" << service.error();
                    return;
                }

                const QString address = service.argumentAt<5>();

                lanLinkProvider->sendUdpIdentityPacket(QList<QHostAddress>{QHostAddress(address)});
                qCDebug(KDECONNECT_CORE) << "Discovered" << name << "at" << address;
            });
}

AvahiDiscovery::~AvahiDiscovery()
{
    onStop();
}

void AvahiDiscovery::onStart()
{
    startAnnouncing();
    startDiscovering();
}

void AvahiDiscovery::onStop()
{
    stopAnnouncing();
    stopDiscovering();
}

void AvahiDiscovery::onNetworkChange()
{
    stopDiscovering();
    startDiscovering();
}

void AvahiDiscovery::startAnnouncing()
{
    if (!m_avahiServerInterface.isValid()) {
        qCWarning(KDECONNECT_CORE) << "Could not connect to Avahi server interface" << m_avahiServerInterface.lastError();
        return;
    }

    if (m_entryGroupInterface) {
        qCWarning(KDECONNECT_CORE) << "Avahi discovery already started";
        return;
    }

    QDBusPendingReply<QDBusObjectPath> reply = m_avahiServerInterface.EntryGroupNew();
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not create Avahi entry group:" << reply.error();
        return;
    }
    m_entryGroupInterface =
        new OrgFreedesktopAvahiEntryGroupInterface(QStringLiteral("org.freedesktop.Avahi"), reply.value().path(), QDBusConnection::systemBus());

    KdeConnectConfig &config = KdeConnectConfig::instance();
    QByteArrayList txtRecords;
    txtRecords.append((QStringLiteral("id=") + config.deviceId()).toUtf8());
    txtRecords.append((QStringLiteral("name=") + config.name()).toUtf8());
    txtRecords.append((QStringLiteral("type=") + config.deviceType().toString()).toUtf8());
    txtRecords.append((QStringLiteral("protocol=") + QString::number(NetworkPacket::s_protocolVersion)).toUtf8());

    auto addServiceReply = m_entryGroupInterface->AddService(-1, // AVAHI_IF_UNSPEC
                                                             -1, // AVAHI_PROTO_UNSPEC
                                                             64, // AVAHI_PUBLISH_UPDATE
                                                             config.name(),
                                                             kServiceType,
                                                             QString(),
                                                             QString(),
                                                             LanLinkProvider::UDP_PORT,
                                                             txtRecords);

    addServiceReply.waitForFinished();
    if (addServiceReply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not add Avahi service:" << addServiceReply.error();
        delete m_entryGroupInterface;
        m_entryGroupInterface = nullptr;
        return;
    }
    auto commitReply = m_entryGroupInterface->Commit();
    commitReply.waitForFinished();
    if (commitReply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not commit Avahi service:" << commitReply.error();
        delete m_entryGroupInterface;
        m_entryGroupInterface = nullptr;
        return;
    }
    qCDebug(KDECONNECT_CORE) << "Started Avahi announcing for device" << config.deviceId();
}

void AvahiDiscovery::stopAnnouncing()
{
    if (!m_avahiServerInterface.isValid()) {
        qCWarning(KDECONNECT_CORE) << "Could not connect to Avahi server interface" << m_avahiServerInterface.lastError();
        return;
    }

    if (!m_entryGroupInterface) {
        qCWarning(KDECONNECT_CORE) << "Avahi discovery already stopped";
        return;
    }

    auto freeReply = m_entryGroupInterface->Free();
    freeReply.waitForFinished();
    if (freeReply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not free Avahi entry group:" << freeReply.error();
    }
    delete m_entryGroupInterface;
    m_entryGroupInterface = nullptr;
    qCDebug(KDECONNECT_CORE) << "Stopped Avahi announcing";
}

void AvahiDiscovery::startDiscovering()
{
}

void AvahiDiscovery::stopDiscovering()
{
}

#include "moc_avahidiscovery.cpp"
