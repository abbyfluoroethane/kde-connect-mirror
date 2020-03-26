/**
 * Copyright 2020 T.H.Arjun <arjunth2001@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "NetworkStatusPlugin.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <core/daemon.h>


K_PLUGIN_CLASS_WITH_JSON(NetworkStatusPlugin, "kdeconnect_NetworkStatusPlugin.json")

Q_LOGGING_CATEGORY(KDECONNECT_PLUGIN_NETWORKSTATUSPLUGIN, "kdeconnect.plugin.NetworkStatusPlugin")

NetworkStatusPlugin:: NetworkStatusPlugin(QObject* parent, const QVariantList& args)
    : KdeConnectPlugin(parent, args)
{

}

void NetworkStatusPlugin::connected()
{
    NetworkPacket np(PACKET_TYPE_NETWORK_STATUS_REQUEST, {{QStringLiteral("request"),true}});
    sendPacket(np);
}

NetworkStatusPlugin::~NetworkStatusPlugin()
{
}

bool NetworkStatusPlugin::receivePacket(const NetworkPacket& np)
{
   qDebug()<<"Network Status Plugin Received a Packet"<< np<<endl;
   return true;

}

#include "NetworkStatusPlugin.moc"