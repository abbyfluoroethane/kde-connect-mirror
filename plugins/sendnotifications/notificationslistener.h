/**
 * SPDX-FileCopyrightText: 2015 Holger Kaelberer <holger.k@elberer.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef NOTIFICATIONSLISTENER_H
#define NOTIFICATIONSLISTENER_H

#include <core/device.h>

class KdeConnectPlugin;
class Notification;
struct NotifyingApplication;

#define PACKET_TYPE_NOTIFICATION QStringLiteral("kdeconnect.notification")

class NotificationsListener : public QObject
{
    Q_OBJECT

public:
    explicit NotificationsListener(KdeConnectPlugin *aPlugin);
    ~NotificationsListener() override;

protected:
    KdeConnectPlugin *m_plugin;
    QHash<QString, NotifyingApplication> m_applications;

    QString m_translatedAppName;

private Q_SLOTS:
    void loadApplications();

private:
    void setTranslatedAppName();
};

#endif // NOTIFICATIONSLISTENER_H
