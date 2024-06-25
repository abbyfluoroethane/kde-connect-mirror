/**
 * SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDBusObjectPath>
#include <QObject>

#include <memory>

class OrgFreedesktopPortalInputCaptureInterface;
class QSocketNotifier;
class OrgFreedesktopPortalSessionInterface;
class Xkb;
struct ei;
struct ei_event;

class InputCaptureSession : public QObject
{
    Q_OBJECT
public:
    InputCaptureSession(QObject *parent);
    ~InputCaptureSession();

    void release(const QPointF &position);
Q_SIGNALS:
    void started(const QPointF &position);
    void mouseMove(double dx, double dy);
    void mouseButton(int button, bool pressed);
    void scrollDelta(double dx, double dy);
    void scrollDiscrete(int dx, int dy);
    void key(Qt::Key key, Qt::KeyboardModifiers modifiers, const QString &text);

private:
    void getZones();
    void enable();

    void sessionCreated(uint response, const QVariantMap &options);
    void zonesReceived(uint response, const QVariantMap &results);
    void barriersSet(uint response, const QVariantMap &results);

    void sessionClosed();
    void disabled(const QDBusObjectPath &sessionHandle, const QVariantMap &options);
    void activated(const QDBusObjectPath &sessionHandle, const QVariantMap &options);
    void deactivated(const QDBusObjectPath &sessionHandle, const QVariantMap &options);
    void zonesChanged(const QDBusObjectPath &sessionHandle, const QVariantMap &options);

    void setupEi(int fd);
    void handleEiEvent(ei_event *event);

    uint m_currentZoneSet = 0;
    uint m_currentActivationId = 0;
    uint m_currentEisSequence = 0;
    QList<ei_event *> queuedEiEvents;
    std::unique_ptr<QSocketNotifier> m_eiNotifier;
    std::unique_ptr<OrgFreedesktopPortalSessionInterface> m_session;
    std::unique_ptr<Xkb> m_xkb;
    ei *m_ei;
    OrgFreedesktopPortalInputCaptureInterface *m_inputCapturePortal;
};
