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
#include <QDBusPendingCallWatcher>
#include <QDBusReply>

#include "generated/systeminterfaces/dbusproperties.h"
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
            // Pause every MPRIS player (Pause has no effect on players that aren't playing) and
            // remember which players were playing, so they can be resumed when the call ends.
            // Pause and Play are already asynchronous, and only PlaybackStatus is read
            // asynchronously here, so an unresponsive player (e.g. Chromium's MPRIS service)
            // can't block the daemon's event loop.
            const QStringList interfaces = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
            for (const QString &iface : interfaces) {
                if (!iface.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
                    continue;
                }
                if (iface.startsWith(QStringLiteral("org.mpris.MediaPlayer2.kdeconnect."))) {
                    continue;
                }
                // Both ringing and talking events trigger this, so skip players already handled.
                if (pausedSources.contains(iface)) {
                    continue;
                }

                OrgMprisMediaPlayer2PlayerInterface mprisInterface(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QDBusConnection::sessionBus());
                mprisInterface.Pause();

                OrgFreedesktopDBusPropertiesInterface props(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QDBusConnection::sessionBus());
                QDBusPendingCallWatcher *statusWatcher =
                    new QDBusPendingCallWatcher(props.Get(QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("PlaybackStatus")), this);
                connect(statusWatcher, &QDBusPendingCallWatcher::finished, this, [this, iface](QDBusPendingCallWatcher *watcher) {
                    watcher->deleteLater();
                    QDBusReply<QVariant> reply = watcher->reply();
                    // Guard against duplicate inserts when ringing and talking events overlap.
                    if (!reply.isValid() || !m_pauseActive || pausedSources.contains(iface)) {
                        return;
                    }
                    if (reply.value().toString() == QLatin1String("Playing")) {
                        pausedSources.insert(iface);
                    }
                });
            }
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
                    mprisInterface.Play();
                }
            }
            pausedSources.clear();
        }
    }
}

#include "moc_pausemusicplugin.cpp"
#include "pausemusicplugin.moc"
