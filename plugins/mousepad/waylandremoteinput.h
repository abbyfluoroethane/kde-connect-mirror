/**
 * SPDX-FileCopyrightText: 2018 Albert Vaca Cintora <albertvaka@gmail.com>
 * SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef WAYLANDREMOTEINPUT_H
#define WAYLANDREMOTEINPUT_H

#include <QDBusObjectPath>
#include "abstractremoteinput.h"
#include "xdp_dbus_remotedesktop_interface.h"

class FakeInput;

class WaylandRemoteInput : public AbstractRemoteInput
{
    Q_OBJECT

public:
    explicit WaylandRemoteInput(QObject *parent);
    ~WaylandRemoteInput();

    bool handlePacket(const NetworkPacket &np) override;

private Q_SLOTS:
    void handleXdpSessionCreated(uint code, const QVariantMap& results);

private:
    void createSession();

    OrgFreedesktopPortalRemoteDesktopInterface* const iface;
    bool m_waylandAuthenticationRequested = false;
    QDBusObjectPath path;
};

#endif
