/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "pausemusicplugin.h"

#include <KPluginFactory>
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Sink>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>

#include "generated/systeminterfaces/mprisplayer.h"
#include <dbushelper.h>

#include "plugin_pausemusic_debug.h"

K_PLUGIN_CLASS_WITH_JSON(PauseMusicPlugin, "kdeconnect_pausemusic.json")

PauseMusicPlugin::PauseMusicPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , mutedSinks()
{
}

void PauseMusicPlugin::receivePacket(const NetworkPacket &np)
{
    bool pauseOnlyWhenTalking = config()->getBool(QStringLiteral("conditionTalking"), false);

    if (pauseOnlyWhenTalking) {
        if (np.get<QString>(QStringLiteral("event")) != QLatin1String("talking")) {
            return;
        }
    } else { // Pause as soon as it rings
        if (np.get<QString>(QStringLiteral("event")) != QLatin1String("ringing") && np.get<QString>(QStringLiteral("event")) != QLatin1String("talking")) {
            return;
        }
    }

    bool pauseConditionFulfilled = !np.get<bool>(QStringLiteral("isCancel"));

    bool pause = config()->getBool(QStringLiteral("actionPause"), true);
    bool mute = config()->getBool(QStringLiteral("actionMute"), false);

    const bool autoResume = config()->getBool(QStringLiteral("actionResume"), true);

    if (pauseConditionFulfilled) {
        m_pauseActive = true;

        if (mute) {
            qCDebug(KDECONNECT_PLUGIN_PAUSEMUSIC) << "Muting system volume";
            const auto sinks = PulseAudioQt::Context::instance()->sinks();
            for (const auto sink : sinks) {
                if (!sink->isMuted()) {
                    sink->setMuted(true);
                    mutedSinks.insert(sink->name());
                }
            }
        }

        if (pause) {
            // Search for interfaces currently playing, asynchronously so a slow (eg: Chromium's MPRIS service)
            // or unresponsive player can't block the daemon's event loop.
            QDBusPendingCall listCall = QDBusConnection::sessionBus().interface()->asyncCall(QStringLiteral("ListNames"));
            QDBusPendingCallWatcher *listWatcher = new QDBusPendingCallWatcher(listCall, this);
            connect(listWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
                watcher->deleteLater();
                QDBusReply<QStringList> reply = watcher->reply();
                if (!reply.isValid() || !m_pauseActive) {
                    return;
                }
                const QStringList interfaces = reply.value();
                for (const QString &iface : interfaces) {
                    if (!iface.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
                        continue;
                    }
                    if (iface.startsWith(QStringLiteral("org.mpris.MediaPlayer2.kdeconnect."))) {
                        continue;
                    }
                    pausePlayer(iface);
                }
            });
        }

    } else {
        m_pauseActive = false;

        if (mute) {
            qCDebug(KDECONNECT_PLUGIN_PAUSEMUSIC) << "Unmuting system volume";

            if (autoResume) {
                const auto sinks = PulseAudioQt::Context::instance()->sinks();
                for (const auto sink : sinks) {
                    if (mutedSinks.contains(sink->name())) {
                        sink->setMuted(false);
                    }
                }
            }
            mutedSinks.clear();
        }

        if (pause && !pausedSources.empty()) {
            if (autoResume) {
                for (const QString &iface : std::as_const(pausedSources)) {
                    OrgMprisMediaPlayer2PlayerInterface mprisInterface(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QDBusConnection::sessionBus());
                    mprisInterface.asyncCall(QStringLiteral("Play"));
                }
            }
            pausedSources.clear();
        }
    }
}

void PauseMusicPlugin::pausePlayer(const QString &iface)
{
    // This function might be called twice in a row because of both ringing and talking events, so ignore duplicate queries.
    if (pausedSources.contains(iface)) {
        return;
    }

    QDBusInterface props(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QStringLiteral("org.freedesktop.DBus.Properties"), QDBusConnection::sessionBus());
    QDBusPendingCall statusCall = props.asyncCall(QStringLiteral("Get"),
                                                  QVariantList{QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("PlaybackStatus")});
    QDBusPendingCallWatcher *statusWatcher = new QDBusPendingCallWatcher(statusCall, this);
    connect(statusWatcher, &QDBusPendingCallWatcher::finished, this, [this, iface](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        QDBusReply<QVariant> reply = watcher->reply();
        if (!reply.isValid() || !m_pauseActive) {
            return;
        }
        // This function might be used by 2 simultaneous events at a time, so guard the insert() by checking contains() first. 
        if (pausedSources.contains(iface)) {
            return;
        }
        if (reply.value().toString() != QLatin1String("Playing")) {
            return;
        }
        pausedSources.insert(iface);
        pauseOrStopPlayer(iface);
    });
}

void PauseMusicPlugin::pauseOrStopPlayer(const QString &iface)
{
    QDBusInterface props(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QStringLiteral("org.freedesktop.DBus.Properties"), QDBusConnection::sessionBus());
    QDBusPendingCall canPauseCall = props.asyncCall(QStringLiteral("Get"),
                                                    QVariantList{QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("CanPause")});
    QDBusPendingCallWatcher *canPauseWatcher = new QDBusPendingCallWatcher(canPauseCall, this);
    connect(canPauseWatcher, &QDBusPendingCallWatcher::finished, this, [this, iface](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        QDBusReply<QVariant> reply = watcher->reply();
        if (!m_pauseActive) {
            return;
        }
        const bool canPause = reply.isValid() && reply.value().toBool();

        OrgMprisMediaPlayer2PlayerInterface mprisInterface(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QDBusConnection::sessionBus());
        if (canPause) {
            mprisInterface.asyncCall(QStringLiteral("Pause"));
        } else {
            mprisInterface.asyncCall(QStringLiteral("Stop"));
        }
    });
}

#include "moc_pausemusicplugin.cpp"
#include "pausemusicplugin.moc"
