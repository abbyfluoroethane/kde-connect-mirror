/**
 * SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "waylandremoteinput.h"

#include <QDebug>
#include <QSizeF>

#include <KLocalizedString>
#include <QDBusPendingCallWatcher>

#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>

namespace
{
//Translation table to keep in sync within all the implementations
int SpecialKeysMap[] = {
    0,               // Invalid
    KEY_BACKSPACE,   // 1
    KEY_TAB,         // 2
    KEY_LINEFEED,    // 3
    KEY_LEFT,        // 4
    KEY_UP,          // 5
    KEY_RIGHT,       // 6
    KEY_DOWN,        // 7
    KEY_PAGEUP,      // 8
    KEY_PAGEDOWN,    // 9
    KEY_HOME,        // 10
    KEY_END,         // 11
    KEY_ENTER,       // 12
    KEY_DELETE,      // 13
    KEY_ESC,         // 14
    KEY_SYSRQ,       // 15
    KEY_SCROLLLOCK,  // 16
    0,               // 17
    0,               // 18
    0,               // 19
    0,               // 20
    KEY_F1,          // 21
    KEY_F2,          // 22
    KEY_F3,          // 23
    KEY_F4,          // 24
    KEY_F5,          // 25
    KEY_F6,          // 26
    KEY_F7,          // 27
    KEY_F8,          // 28
    KEY_F9,          // 29
    KEY_F10,         // 30
    KEY_F11,         // 31
    KEY_F12,         // 32
};
}

WaylandRemoteInput::WaylandRemoteInput(QObject* parent)
    : AbstractRemoteInput(parent)
    , iface(new OrgFreedesktopPortalRemoteDesktopInterface(
        QLatin1String("org.freedesktop.portal.Desktop"), QLatin1String("/org/freedesktop/portal/desktop"), QDBusConnection::sessionBus(), this))
{
}

void WaylandRemoteInput::createSession()
{
    // create session
    qDebug() << "creating session...";
    const auto handleToken = QStringLiteral("kdeconnect%1").arg(QRandomGenerator::global()->generate());
    const auto sessionParameters = QVariantMap {
        { QLatin1String("session_handle_token"), handleToken },
        { QLatin1String("handle_token"), handleToken }
    };
    auto sessionReply = iface->CreateSession(sessionParameters);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(sessionReply);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, sessionReply] (QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (sessionReply.isError()) {
            qCWarning(KDECONNECT_PLUGIN_MOUSEPAD) << "Could not create the remote control session" << sessionReply.error();
        }

        //ASYNC
        bool b =
        QDBusConnection::sessionBus().connect(QString(),
                                            sessionReply.value().path(),
                                            QLatin1String("org.freedesktop.portal.Request"),
                                            QLatin1String("Response"),
                                            this,
                                            SLOT(handleXdpSessionCreated(uint, QVariantMap)));
        Q_ASSERT(b);

        qCDebug(KDECONNECT_PLUGIN_MOUSEPAD) << "authenticating" << sessionReply.value().path();
    });
}

void WaylandRemoteInput::handleXdpSessionCreated(uint code, const QVariantMap& results)
{
    qDebug() << "session created with" << code << results;
    path = QDBusObjectPath(results.value(QLatin1String("session_handle")).toString());
    const auto startParameters = QVariantMap {
        { QLatin1String("handle_token"), QStringLiteral("kdeconnect%1").arg(QRandomGenerator::global()->generate()) }
    };

    auto reply = iface->Start(path, {}, startParameters);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [reply] (QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (reply.isError()) {
            qCWarning(KDECONNECT_PLUGIN_MOUSEPAD) << "Could not start the remote control session" << reply.error();
        }
    });
}

WaylandRemoteInput::~WaylandRemoteInput()
{
}

bool WaylandRemoteInput::handlePacket(const NetworkPacket &np)
{
    if (!m_waylandAuthenticationRequested) {
        m_waylandAuthenticationRequested = true;
        createSession();
        return false;
    } else if (path.path().isEmpty()) {
        qCWarning(KDECONNECT_PLUGIN_MOUSEPAD) << "remotedesktop portal not authenticated";
        return false;
    }


    const float dx = np.get<float>(QStringLiteral("dx"), 0);
    const float dy = np.get<float>(QStringLiteral("dy"), 0);

    const bool isSingleClick = np.get<bool>(QStringLiteral("singleclick"), false);
    const bool isDoubleClick = np.get<bool>(QStringLiteral("doubleclick"), false);
    const bool isMiddleClick = np.get<bool>(QStringLiteral("middleclick"), false);
    const bool isRightClick = np.get<bool>(QStringLiteral("rightclick"), false);
    const bool isSingleHold = np.get<bool>(QStringLiteral("singlehold"), false);
    const bool isSingleRelease = np.get<bool>(QStringLiteral("singlerelease"), false);
    const bool isScroll = np.get<bool>(QStringLiteral("scroll"), false);
    const QString key = np.get<QString>(QStringLiteral("key"), QLatin1String(""));
    const int specialKey = np.get<int>(QStringLiteral("specialKey"), 0);

    if (isSingleClick || isDoubleClick || isMiddleClick || isRightClick || isSingleHold || isSingleRelease || isScroll || !key.isEmpty() || specialKey) {
        if (isSingleClick) {
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 1);
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 0);
        } else if (isDoubleClick) {
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 1);
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 0);
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 1);
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 0);
        } else if (isMiddleClick) {
            iface->NotifyPointerButton(path, {}, BTN_MIDDLE, 1);
            iface->NotifyPointerButton(path, {}, BTN_MIDDLE, 0);
        } else if (isRightClick) {
            iface->NotifyPointerButton(path, {}, BTN_RIGHT, 1);
            iface->NotifyPointerButton(path, {}, BTN_RIGHT, 0);
        } else if (isSingleHold){
            //For drag'n drop
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 1);
        } else if (isSingleRelease){
            //For drag'n drop. NEVER USED (release is done by tapping, which actually triggers a isSingleClick). Kept here for future-proofness.
            iface->NotifyPointerButton(path, {}, BTN_LEFT, 0);
        } else if (isScroll) {
            iface->NotifyPointerAxis(path, {}, dx, dy);
        } else if (specialKey) {
            iface->NotifyKeyboardKeycode(path, {}, SpecialKeysMap[specialKey], 1);
            iface->NotifyKeyboardKeycode(path, {}, SpecialKeysMap[specialKey], 0);
        } else if (!key.isEmpty()) {
            for (const QChar character : key) {
                const auto keysym = xkb_utf32_to_keysym(character.unicode());
                iface->NotifyKeyboardKeysym(path, {}, keysym, 1);
                iface->NotifyKeyboardKeysym(path, {}, keysym, 0);
            }
        }


    } else { // Is a mouse move event
        iface->NotifyPointerMotion(path, {}, dx, dy);
    }
    return true;
}
