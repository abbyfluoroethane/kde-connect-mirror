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
    /*
    connect(&mdnsDiscoverer, &MdnshWrapper::Discoverer::serviceFound, this, [lanLinkProvider](const MdnshWrapper::Discoverer::MdnsService &service) {
        if (KdeConnectConfig::instance().deviceId() == service.name) {
            qCDebug(KDECONNECT_CORE) << "Discovered myself, ignoring";
            return;
        }
        lanLinkProvider->sendUdpIdentityPacket(QList<QHostAddress>{service.address});
        qCDebug(KDECONNECT_CORE) << "Discovered" << service.name << "at" << service.address;
    });
    */
}

AvahiDiscovery::~AvahiDiscovery()
{
    onStop();
}

void AvahiDiscovery::onStart()
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

    // mdnsAnnouncer.startAnnouncing();
    // mdnsDiscoverer.startDiscovering(kServiceType);
}

void AvahiDiscovery::onStop()
{
    if (m_entryGroupInterface == nullptr) {
        qCWarning(KDECONNECT_CORE) << "Avahi discovery already stopped";
        return;
    }
    // mdnsAnnouncer.stopAnnouncing();
    // mdnsDiscoverer.stopDiscovering();
}

void AvahiDiscovery::onNetworkChange()
{
    // mdnsAnnouncer.onNetworkChange();
    // mdnsDiscoverer.stopDiscovering();
    // mdnsDiscoverer.startDiscovering(kServiceType);
}

#include "moc_avahidiscovery.cpp"
