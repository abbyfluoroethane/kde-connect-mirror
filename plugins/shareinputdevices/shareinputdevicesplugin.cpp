/**
 * SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "shareinputdevicesplugin.h"

#include "inputcapturesession.h"
#include "plugin_shareinputdevices_debug.h"

#include <core/daemon.h>
#include <core/device.h>

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusConnection>
#include <QGuiApplication>
#include <QPointF>
#include <QScreen>

#include <linux/input-event-codes.h>

using namespace Qt::StringLiterals;

#define PACKET_TYPE_MOUSEPAD_REQUEST u"kdeconnect.mousepad.request"_s
#define PACKET_TYPE_SHAREINPUTDEVICES u"kdeconnect.shareinputdevices"_s

QMap<int, int> specialKeysMap = {
    // 0,              // Invalid
    {Qt::Key_Backspace, 1},
    {Qt::Key_Tab, 2},
    // XK_Linefeed,    // 3
    {Qt::Key_Left, 4},
    {Qt::Key_Up, 5},
    {Qt::Key_Right, 6},
    {Qt::Key_Down, 7},
    {Qt::Key_PageUp, 8},
    {Qt::Key_PageDown, 9},
    {Qt::Key_Home, 10},
    {Qt::Key_End, 11},
    {Qt::Key_Return, 12},
    {Qt::Key_Enter, 12},
    {Qt::Key_Delete, 13},
    {Qt::Key_Escape, 14},
    {Qt::Key_SysReq, 15},
    {Qt::Key_ScrollLock, 16},
    // 0,              // 17
    // 0,              // 18
    // 0,              // 19
    // 0,              // 20
    {Qt::Key_F1, 21},
    {Qt::Key_F2, 22},
    {Qt::Key_F3, 23},
    {Qt::Key_F4, 24},
    {Qt::Key_F5, 25},
    {Qt::Key_F6, 26},
    {Qt::Key_F7, 27},
    {Qt::Key_F8, 28},
    {Qt::Key_F9, 29},
    {Qt::Key_F10, 30},
    {Qt::Key_F11, 31},
    {Qt::Key_F12, 32},
};

K_PLUGIN_CLASS_WITH_JSON(ShareInputDevicesPlugin, "kdeconnect_shareinputdevices.json")

ShareInputDevicesPlugin::ShareInputDevicesPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_inputCaptureSession(new InputCaptureSession(this))
{
    connect(m_inputCaptureSession, &InputCaptureSession::started, this, [this](const QLine &barrier, const QPointF &delta) {
        m_activatedBarrier = barrier;
        NetworkPacket packet(PACKET_TYPE_SHAREINPUTDEVICES, {{u"startEdge"_s, configuredEdge()}, {u"delta"_s, delta}});
        sendPacket(packet);
    });
    connect(m_inputCaptureSession, &InputCaptureSession::mouseMove, this, [this](double x, double y) {
        NetworkPacket packet(PACKET_TYPE_MOUSEPAD_REQUEST, {{u"dx"_s, x}, {u"dy"_s, y}});
        sendPacket(packet);
    });
    connect(m_inputCaptureSession, &InputCaptureSession::mouseButton, this, [this](int button, bool pressed) {
        NetworkPacket packet(PACKET_TYPE_MOUSEPAD_REQUEST);
        // mousepad only supports separate press and release for left click currently, so send event on release even though not entirely correct
        if (button == BTN_LEFT) {
            packet.set(pressed ? u"singlehold"_s : u"singlerelease"_s, true);
        } else if (button == BTN_RIGHT && pressed) {
            packet.set(u"rightclick"_s, true);
        } else if (button == BTN_MIDDLE) {
            packet.set(u"middleclick"_s, true);
        }
        sendPacket(packet);
    });
    connect(m_inputCaptureSession, &InputCaptureSession::scrollDelta, this, [this](double x, double y) {
        NetworkPacket packet(PACKET_TYPE_MOUSEPAD_REQUEST, {{u"scroll"_s, true}, {u"dx"_s, x}, {u"dy"_s, y}});
        sendPacket(packet);
    });
    connect(m_inputCaptureSession, &InputCaptureSession::scrollDiscrete, this, [this](int x, int y) {
        constexpr auto anglePer120Step = 15 / 120.0;
        NetworkPacket packet(PACKET_TYPE_MOUSEPAD_REQUEST, {{u"scroll"_s, true}, {u"dx"_s, x * anglePer120Step}, {u"dy"_s, y * anglePer120Step}});
        sendPacket(packet);
    });
    connect(m_inputCaptureSession, &InputCaptureSession::key, this, [this](Qt::Key key, Qt::KeyboardModifiers modifiers, const QString &text) {
        NetworkPacket packet(PACKET_TYPE_MOUSEPAD_REQUEST,
                             {
                                 {u"key"_s, text},
                                 {u"specialKey"_s, specialKeysMap.value(key)},
                                 {u"shift"_s, static_cast<bool>(modifiers & Qt::ShiftModifier)},
                                 {u"ctr"_s, static_cast<bool>(modifiers & Qt::ControlModifier)},
                                 {u"alt"_s, static_cast<bool>(modifiers & Qt::AltModifier)},
                                 {u"super"_s, static_cast<bool>(modifiers & Qt::MetaModifier)},
                             });
        sendPacket(packet);
    });

    m_inputCaptureSession->setBarrierEdge(configuredEdge());
    connect(config(), &KdeConnectPluginConfig::configChanged, this, [this] {
        m_inputCaptureSession->setBarrierEdge(configuredEdge());
    });
}

Qt::Edge ShareInputDevicesPlugin::configuredEdge() const
{
    return static_cast<Qt::Edge>(config()->getInt(u"edge"_s, Qt::LeftEdge));
}

void ShareInputDevicesPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() == PACKET_TYPE_SHAREINPUTDEVICES) {
        if (np.has(u"startEdge"_s)) {
            const Qt::Edge startEdge = np.get<Qt::Edge>(u"edge"_s);
            const QPointF delta = np.get<QPointF>(u"delta"_s);
            auto screens = QGuiApplication::screens();
            auto mousePadPlugin = device()->plugin(u"kdeconnect_mousepad"_s);
            if (screens.empty() || !mousePadPlugin) {
                NetworkPacket packet(PACKET_TYPE_SHAREINPUTDEVICES, {{u"releaseDelta"_s, QPointF(0, 0)}});
                sendPacket(packet);
                return;
            }
            // This is opposite to the sorting in inputcaptureession because here the opposite edge is wanted.
            // For example if on the source side a left barrier was crossed, this is one is entered from the right.
            if (startEdge == Qt::LeftEdge) {
                std::stable_sort(screens.begin(), screens.end(), [](QScreen *lhs, QScreen *rhs) {
                    return lhs->geometry().x() + lhs->geometry().width() > rhs->geometry().x() + rhs->geometry().width();
                });
                m_enterEdge = Qt::RightEdge;
                m_currentPosition = screens.front()->geometry().topRight() + delta;
            } else if (startEdge == Qt::RightEdge) {
                std::stable_sort(screens.begin(), screens.end(), [](QScreen *lhs, QScreen *rhs) {
                    return lhs->geometry().x() < rhs->geometry().x();
                });
                m_enterEdge = Qt::LeftEdge;
                m_currentPosition = screens.front()->geometry().topLeft() + delta;
            } else if (startEdge == Qt::TopEdge) {
                std::stable_sort(screens.begin(), screens.end(), [](QScreen *lhs, QScreen *rhs) {
                    return lhs->geometry().y() + lhs->geometry().height() > rhs->geometry().y() + rhs->geometry().height();
                });
                m_enterEdge = Qt::BottomEdge;
                m_currentPosition = screens.front()->geometry().bottomLeft() + delta;
            } else {
                std::stable_sort(screens.begin(), screens.end(), [](QScreen *lhs, QScreen *rhs) {
                    return lhs->geometry().y() < rhs->geometry().y();
                });
                m_enterEdge = Qt::TopEdge;
                m_currentPosition = screens.front()->geometry().topLeft() + delta;
            }
            m_enterScreen = screens.front();
            qDebug() << "delta" << delta << "enter" << m_currentPosition;
            // Send a packet from this local device to the mousepad plugin because the position could only
            // be calculated here
            NetworkPacket packet(PACKET_TYPE_MOUSEPAD_REQUEST, {{u"x"_s, m_currentPosition.x()}, {u"y"_s, m_currentPosition.y()}});
            mousePadPlugin->receivePacket(packet);
        } else if (np.has(u"releaseDelta"_s)) {
            const QPointF releasePosition = m_activatedBarrier.p1() + np.get<QPointF>(u"releaseDelta"_s);
            m_inputCaptureSession->release(releasePosition);
        }
    }

    if (np.type() == PACKET_TYPE_MOUSEPAD_REQUEST) {
        if (np.has(u"dx"_s) && m_enterScreen) {
            m_currentPosition += QPointF(np.get<double>(u"dx"_s), np.get<double>(u"dy"_s));
            qDebug() << m_currentPosition;
            if (m_enterEdge == Qt::LeftEdge && m_currentPosition.x() < m_enterScreen->geometry().x()) {
                const QPointF delta = m_currentPosition - m_enterScreen->geometry().topLeft();
                NetworkPacket packet(PACKET_TYPE_SHAREINPUTDEVICES, {{u"releaseDelta"_s, delta}});
                sendPacket(packet);
            } else if (m_enterEdge == Qt::RightEdge && m_currentPosition.x() > m_enterScreen->geometry().x() + m_enterScreen->geometry().width()) {
                const QPointF delta = m_currentPosition - m_enterScreen->geometry().topRight();
                NetworkPacket packet(PACKET_TYPE_SHAREINPUTDEVICES, {{u"releaseDelta"_s, delta}});
                sendPacket(packet);
            } else if (m_enterEdge == Qt::TopEdge && m_currentPosition.y() < m_enterScreen->geometry().y()) {
                const QPointF delta = m_currentPosition - m_enterScreen->geometry().topLeft();
                NetworkPacket packet(PACKET_TYPE_SHAREINPUTDEVICES, {{u"releaseDelta"_s, delta}});
                sendPacket(packet);
            } else if (m_enterEdge == Qt::BottomEdge && m_currentPosition.y() > m_enterScreen->geometry().y() + m_enterScreen->geometry().height()) {
                const QPointF delta = m_currentPosition - m_enterScreen->geometry().bottomLeft();
                NetworkPacket packet(PACKET_TYPE_SHAREINPUTDEVICES, {{u"releaseDelta"_s, delta}});
                sendPacket(packet);
            }
        }
    }
}

QString ShareInputDevicesPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/shareinputdevices").arg(device()->id());
}

#include "shareinputdevicesplugin.moc"
