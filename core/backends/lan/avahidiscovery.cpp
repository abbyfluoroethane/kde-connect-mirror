/**
 * SPDX-FileCopyrightText: 2025 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "avahidiscovery.h"

#include "core_debug.h"
#include "kdeconnectconfig.h"
#include "lanlinkprovider.h"

const QString kAvahiDbusService = QStringLiteral("org.freedesktop.Avahi");
const QString kKdeConnectServiceType = QStringLiteral("_kdeconnect._udp");

enum {
    AVAHI_SERVER_INVALID,
    AVAHI_SERVER_REGISTERING,
    AVAHI_SERVER_RUNNING,
    AVAHI_SERVER_COLLISION,
    AVAHI_SERVER_FAILURE,
};

enum {
    AVAHI_ENTRY_GROUP_UNCOMMITTED,
    AVAHI_ENTRY_GROUP_REGISTERING,
    AVAHI_ENTRY_GROUP_ESTABLISHED,
    AVAHI_ENTRY_GROUP_COLLISION,
    AVAHI_ENTRY_GROUP_FAILURE,
};

AvahiDiscovery::AvahiDiscovery(LanLinkProvider *lanLinkProvider)
    : m_avahiServerInterface(kAvahiDbusService, QStringLiteral("/"), QDBusConnection::systemBus())
    , m_serviceBrowserInterface(nullptr)
    , m_entryGroupInterface(nullptr)
    , lanLinkProvider(lanLinkProvider)
{
    connect(&m_avahiServerInterface, &OrgFreedesktopAvahiServer2Interface::StateChanged, this, [](int state, const QString &error) {
        if (state == AVAHI_SERVER_COLLISION) {
            qCWarning(KDECONNECT_CORE) << "Avahi server collision" << error;
        } else if (state == AVAHI_SERVER_FAILURE) {
            qCWarning(KDECONNECT_CORE) << "Avahi server failure " << error;
        }
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

    QDBusPendingReply<QDBusObjectPath> pathReply = m_avahiServerInterface.EntryGroupNew();
    pathReply.waitForFinished();
    if (pathReply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not create Avahi entry group:" << pathReply.error();
        return;
    }
    m_entryGroupInterface = new OrgFreedesktopAvahiEntryGroupInterface(kAvahiDbusService, pathReply.value().path(), QDBusConnection::systemBus());

    if (!m_entryGroupInterface->isValid()) {
        qCWarning(KDECONNECT_CORE) << "Could not connect to Avahi entity group" << m_entryGroupInterface->lastError();
        return;
    }

    connect(m_entryGroupInterface, &OrgFreedesktopAvahiEntryGroupInterface::StateChanged, this, [](int state, const QString &error) {
        if (state == AVAHI_ENTRY_GROUP_COLLISION) {
            qCWarning(KDECONNECT_CORE) << "Avahi entry group collision" << error;
        } else if (state == AVAHI_ENTRY_GROUP_FAILURE) {
            qCWarning(KDECONNECT_CORE) << "Avahi entry group failure " << error;
        }
    });

    KdeConnectConfig &config = KdeConnectConfig::instance();
    QByteArrayList txtRecords;
    txtRecords.append((QStringLiteral("id=") + config.deviceId()).toUtf8());
    txtRecords.append((QStringLiteral("name=") + config.name()).toUtf8());
    txtRecords.append((QStringLiteral("type=") + config.deviceType().toString()).toUtf8());
    txtRecords.append((QStringLiteral("protocol=") + QString::number(NetworkPacket::s_protocolVersion)).toUtf8());

    auto addServiceReply = m_entryGroupInterface->AddService(-1, // interface: AVAHI_IF_UNSPEC
                                                             -1, // protocol: AVAHI_PROTO_UNSPEC
                                                             64, // flags: AVAHI_PUBLISH_UPDATE
                                                             config.name(), // name
                                                             kKdeConnectServiceType, // type
                                                             QString(), // domain
                                                             QString(), // host
                                                             LanLinkProvider::UDP_PORT, // port
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
    if (!m_entryGroupInterface) {
        qCWarning(KDECONNECT_CORE) << "Avahi announce already stopped";
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
    if (!m_avahiServerInterface.isValid()) {
        qCWarning(KDECONNECT_CORE) << "Could not connect to Avahi server interface" << m_avahiServerInterface.lastError();
        return;
    }

    QDBusPendingReply<QDBusObjectPath> pathReply = m_avahiServerInterface.ServiceBrowserPrepare(-1, // interface: AVAHI_IF_UNSPEC
                                                                                                -1, // protocol: AVAHI_PROTO_UNSPEC
                                                                                                kKdeConnectServiceType, // type
                                                                                                QString(), // domain
                                                                                                0 // flags
    );
    pathReply.waitForFinished();
    if (pathReply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not create Avahi entry group:" << pathReply.error();
        return;
    }

    m_serviceBrowserInterface = new OrgFreedesktopAvahiServiceBrowserInterface(kAvahiDbusService, pathReply.value().path(), QDBusConnection::systemBus());

    // Discovery callback
    connect(m_serviceBrowserInterface,
            &OrgFreedesktopAvahiServiceBrowserInterface::ItemNew,
            this,
            [this](int interface, int protocol, const QString &name, const QString &type, const QString &domain) {
                if (KdeConnectConfig::instance().deviceId() == name) {
                    qCDebug(KDECONNECT_CORE) << "Discovered myself, ignoring";
                    return;
                }

                // interface, protocol, name, type, domain, host, aprotocol, address, port, txt, flags
                QDBusPendingReply<int, int, QString, QString, QString, QString, int, QString, ushort, QByteArrayList, uint> service =
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

    connect(m_serviceBrowserInterface, &OrgFreedesktopAvahiServiceBrowserInterface::Failure, this, [](const QString &error) {
        qCWarning(KDECONNECT_CORE) << "Avahi discovery failure " << error;
    });

    if (!m_serviceBrowserInterface->isValid()) {
        qCWarning(KDECONNECT_CORE) << "Could not connect to Avahi browser interface" << m_serviceBrowserInterface->lastError();
        return;
    }

    auto startReply = m_serviceBrowserInterface->Start();
    startReply.waitForFinished();
    if (startReply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Avahi could not start discovering" << startReply.error();
        return;
    }
}

void AvahiDiscovery::stopDiscovering()
{
    if (!m_serviceBrowserInterface) {
        qCWarning(KDECONNECT_CORE) << "Avahi discovery already stopped";
        return;
    }

    auto freeReply = m_serviceBrowserInterface->Free();
    freeReply.waitForFinished();
    if (freeReply.isError()) {
        qCWarning(KDECONNECT_CORE) << "Could not free Avahi browser:" << freeReply.error();
    }
    delete m_serviceBrowserInterface;
    m_serviceBrowserInterface = nullptr;
    qCDebug(KDECONNECT_CORE) << "Stopped Avahi discovery";
}

#include "moc_avahidiscovery.cpp"
