/**
 * SPDX-FileCopyrightText: 2015 Holger Kaelberer <holger.k@elberer.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef NOTIFICATIONSLISTENER_H
#define NOTIFICATIONSLISTENER_H

#include <core/device.h>

#include <QBuffer>
#include <QFile>

#ifdef Q_OS_UNIX
#include <gio/gio.h>
#endif
#if WIN_SDK_VERSION >= 17763
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include <winrt/Windows.UI.Notifications.h>
#endif

class KdeConnectPlugin;
class Notification;
struct NotifyingApplication;

class NotificationsListener : public QObject
{
    Q_OBJECT

public:
    explicit NotificationsListener(KdeConnectPlugin *aPlugin);
    ~NotificationsListener() override;

protected:
    KdeConnectPlugin *m_plugin;
    QHash<QString, NotifyingApplication> m_applications;

    // virtual helper function to make testing possible (QDBusArgument can not
    // be injected without making a DBUS-call):
#ifdef Q_OS_UNIX
    virtual bool parseImageDataArgument(GVariant *argument,
                                        int &width,
                                        int &height,
                                        int &rowStride,
                                        int &bitsPerSample,
                                        int &channels,
                                        bool &hasAlpha,
                                        QByteArray &imageData) const;
    QSharedPointer<QIODevice> iconForImageData(GVariant *argument) const;
    static QSharedPointer<QIODevice> iconForIconName(const QString &iconName);

    static GDBusMessage *onMessageFiltered(GDBusConnection *connection, GDBusMessage *msg, int incoming, void *parent);
#endif

private Q_SLOTS:
    void loadApplications();

private:
#ifdef Q_OS_UNIX
    void setupDBusMonitor();
#endif
#if WIN_SDK_VERSION >= 17763
    void setupWindowsUserNotificationListener();
    void onNotificationChanged(const winrt::Windows::UI::Notifications::Management::UserNotificationListener &sender,
                               const winrt::Windows::UI::Notifications::UserNotificationChangedEventArgs &args);
#endif

    void setTranslatedAppName();
    QString m_translatedAppName;

#ifdef Q_OS_UNIX
    GDBusConnection *m_gdbusConnection = nullptr;
    unsigned m_gdbusFilterId = 0;
#endif
#if WIN_SDK_VERSION >= 17763
    winrt::event_token m_notificationEventToken;
#endif
};

#endif // NOTIFICATIONSLISTENER_H
