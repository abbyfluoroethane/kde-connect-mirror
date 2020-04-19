/**
 * Copyright 2018 Friedrich W. H. Kossebau <kossebau@kde.org>
 * Copyright 2019 Piyush Aggarwal <piyushaggarwal002@gmail.com>
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

#include "findthisdeviceplugin.h"

#include "kdeconnect-version.h"

// KF
#include <KPluginFactory>
#include <KLocalizedString>

#ifndef Q_OS_WIN
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Sink>
#endif

// Qt
#include <QDBusConnection>
#include <QMediaPlayer>

K_PLUGIN_CLASS_WITH_JSON(FindThisDevicePlugin, "kdeconnect_findthisdevice.json")

FindThisDevicePlugin::FindThisDevicePlugin(QObject* parent, const QVariantList& args)
    : KdeConnectPlugin(parent, args)
{
#ifdef HAVE_CANBERRA
    int retCreate = ca_context_create(&m_canberraContext);

    if (retCreate != CA_SUCCESS) {
        qCWarning(KDECONNECT_PLUGIN_FINDTHISDEVICE) << "Failed to create canberra context" << ca_strerror(retCreate);
    }

    int retProps = ca_context_change_props(m_canberraContext,
        CA_PROP_APPLICATION_NAME, qUtf8Printable(i18n("KDE Connect")),
        CA_PROP_APPLICATION_ID, "org.kde.kdeconnect.daemon",
        CA_PROP_APPLICATION_VERSION, KDECONNECT_VERSION_STRING,
        CA_PROP_APPLICATION_ICON_NAME, "kdeconnect",
        nullptr
    );

    if (retProps != CA_SUCCESS) {
        qCWarning(KDECONNECT_PLUGIN_FINDTHISDEVICE) << "Failed to set canberra context properties" << ca_strerror(retProps);
    }

    ca_proplist_create(&m_canberraProperties);
    ca_proplist_sets(m_canberraProperties, CA_PROP_EVENT_ID, "phone-incoming-call");
    ca_proplist_sets(m_canberraProperties, CA_PROP_EVENT_DESCRIPTION, qUtf8Printable(i18n("Device is ringing")));
#endif
}

FindThisDevicePlugin::~FindThisDevicePlugin()
{
#ifdef HAVE_CANBERRA
    ca_context_destroy(m_canberraContext);
    ca_proplist_destroy(m_canberraProperties);
#endif
}

bool FindThisDevicePlugin::receivePacket(const NetworkPacket& np)
{
    Q_UNUSED(np);

    muteSinks();

#ifdef HAVE_CANBERRA
    auto callback = [](ca_context *c, uint32_t id, int error_code, void *userdata) {
        Q_UNUSED(c)
        Q_UNUSED(id)

        if (error_code != CA_SUCCESS) {
            qCWarning(KDECONNECT_PLUGIN_FINDTHISDEVICE) << "Failed to play sound" << ca_strerror(error_code);
        }

        FindThisDevicePlugin* plugin = static_cast<FindThisDevicePlugin*>(userdata);
        plugin->unmuteSinks();
    };

    ca_context_play_full(m_canberraContext, 0, m_canberraProperties, callback, this);
#else
    const QString soundFile = config()->get<QString>(QStringLiteral("ringtone"), defaultSound());
    const QUrl soundURL = QUrl(soundFile);
    if (soundURL.isEmpty()) {
        qCWarning(KDECONNECT_PLUGIN_FINDTHISDEVICE) << "Not playing sound, no valid ring tone specified.";
        return true;
    }

    QMediaPlayer* player = new QMediaPlayer;
    player->setAudioRole(QAudio::Role(QAudio::NotificationRole));
    player->setMedia(soundURL);
    player->setVolume(100);

    connect(player, &QMediaPlayer::stateChanged, this, &FindThisDevicePlugin::unmuteSinks);
    connect(player, &QMediaPlayer::stateChanged, player, &QObject::deleteLater);

    player->play();
    // TODO: ensure to use built-in loudspeakers
#endif
    return true;
}

void FindThisDevicePlugin::muteSinks()
{
#ifndef Q_OS_WIN
    const auto sinks = PulseAudioQt::Context::instance()->sinks();

    for (auto sink : sinks) {
        if (sink->isMuted()) {
            sink->setMuted(false);
            m_mutedSinks.append(sink);
        }
    }
#endif
}

void FindThisDevicePlugin::unmuteSinks()
{
#ifndef Q_OS_WIN
    for (auto sink : qAsConst(m_mutedSinks)) {
        sink->setMuted(true);
    }
    m_mutedSinks.clear();
#endif
}

QString FindThisDevicePlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/") + device()->id() + QStringLiteral("/findthisdevice");
}

#include "findthisdeviceplugin.moc"
