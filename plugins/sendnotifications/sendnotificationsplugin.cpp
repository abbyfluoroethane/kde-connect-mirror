/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "sendnotificationsplugin.h"

#ifdef Q_OS_UNIX
#include "dbusnotificationslistener.h"
#endif
#ifdef Q_OS_WIN
#include "windowsnotificationslistener.h"
#endif
#include "plugin_sendnotification_debug.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(SendNotificationsPlugin, "kdeconnect_sendnotifications.json")

SendNotificationsPlugin::SendNotificationsPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
#ifdef Q_OS_UNIX
    notificationsListener = new DBusNotificationsListener(this);
#endif
#ifdef Q_OS_WIN
    notificationsListener = new WindowsNotificationsListener(this);
#endif
}

SendNotificationsPlugin::~SendNotificationsPlugin()
{
    delete notificationsListener;
}

bool SendNotificationsPlugin::receivePacket(const NetworkPacket &np)
{
    Q_UNUSED(np);
    return true;
}

void SendNotificationsPlugin::connected()
{
}

#include "sendnotificationsplugin.moc"
