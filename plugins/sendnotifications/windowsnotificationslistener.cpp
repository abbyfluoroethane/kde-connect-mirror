/**
 * SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "windowsnotificationslistener.h"

#include <QBuffer>
#include <QIODevice>

#include <core/kdeconnectplugin.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>

#include "notifyingapplication.h"
#include "plugin_sendnotification_debug.h"

using namespace winrt;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Notifications;
using namespace Windows::UI::Notifications::Management;
using namespace Windows::Storage::Streams;
using namespace Windows::Security::Cryptography;

WindowsNotificationsListener::WindowsNotificationsListener(KdeConnectPlugin* aPlugin)
    : NotificationsListener(aPlugin)
{
    setupWindowsUserNotificationListener();
}

WindowsNotificationsListener::~WindowsNotificationsListener()
{
    UserNotificationListener::Current().NotificationChanged(m_notificationEventToken);
}

void WindowsNotificationsListener::setupWindowsUserNotificationListener()
{
    // Register the notification listener
    const UserNotificationListenerAccessStatus accessStatus = UserNotificationListener::Current().RequestAccessAsync().get();
    if (accessStatus != UserNotificationListenerAccessStatus::Allowed) {
        return;
    }

    // Register the event handler for notifications
    m_notificationEventToken = UserNotificationListener::Current().NotificationChanged({ this, &WindowsNotificationsListener::onNotificationChanged });
}

void WindowsNotificationsListener::onNotificationChanged(const UserNotificationListener &sender, const UserNotificationChangedEventArgs &args)
{
    // Get the notification from the event arguments
    const UserNotificationChangedKind changeKind = args.ChangeKind();
    if (changeKind == UserNotificationChangedKind::Removed) {
        return;
    }

    const UserNotification userNotification = sender.GetNotification(args.UserNotificationId());
    if (!userNotification) {
        return;
    }

    const AppDisplayInfo appDisplayInfo = userNotification.AppInfo().DisplayInfo();
    const QString appName = QString::fromStdString(winrt::to_string(appDisplayInfo.DisplayName()));
    if (m_translatedAppName == appName) {
        return;
    }

    if (!appName.isEmpty()) {
        auto it = m_applications.constFind(appName);
        if (it == m_applications.cend()) {
            // new application -> add to config
            NotifyingApplication app;
            app.name = appName;
            app.active = true;
            m_applications.insert(app.name, app);
            // update config:
            QVariantList list;
            for (const auto &a : std::as_const(m_applications)) {
                list << QVariant::fromValue<NotifyingApplication>(a);
            }
            m_plugin->config()->setList(QStringLiteral("applications"), list);
        } else if (!it->active) {
            return;
        }
    }

    const winrt::Windows::UI::Notifications::Notification notification = userNotification.Notification();
    const NotificationBinding toastBinding = notification.Visual().GetBinding(KnownNotificationBindings::ToastGeneric());
    if (!toastBinding) {
        return;
    }
    const auto textElements = toastBinding.GetTextElements();
    if (textElements.Size() == 0) {
        return;
    }

    std::wstring ticker = static_cast<std::wstring>(textElements.GetAt(0).Text());
    if (m_plugin->config()->getBool(QStringLiteral("generalIncludeBody"), true)) {
        for (unsigned i = 1; i < textElements.Size(); ++i) {
            ticker += L"\n" + textElements.GetAt(i).Text();
        }
    }

    static unsigned id = 0;
    NetworkPacket np(PACKET_TYPE_NOTIFICATION,
        { { QStringLiteral("id"), id++ },
            { QStringLiteral("appName"), appName },
            { QStringLiteral("ticker"), QString::fromStdWString(ticker) },
            { QStringLiteral("isClearable"), true } }); // A Windows notification is always clearable

    if (m_plugin->config()->getBool(QStringLiteral("generalSynchronizeIcons"), true)) {
        const RandomAccessStreamReference appLogoStreamReference = appDisplayInfo.GetLogo(Size(64, 64));
        if (appLogoStreamReference) {
            QSharedPointer<QIODevice> iconSource;
            IRandomAccessStreamWithContentType logoStream = appLogoStreamReference.OpenReadAsync().get(); // TODO Port to coroutine

            // Read the logo stream into a buffer
            const uint64_t logoSize = logoStream.Size();
            QByteArray buffer(static_cast<int>(logoSize), Qt::Uninitialized);
            QBuffer qtBuffer(&buffer);
            qtBuffer.open(QIODevice::WriteOnly);
            IBuffer winrtBuffer = CryptographicBuffer::CreateFromByteArray(winrt::array_view<uint8_t>(reinterpret_cast<uint8_t*>(buffer.data()), buffer.size()));
            logoStream.ReadAsync(winrtBuffer, static_cast<uint32_t>(logoSize), InputStreamOptions::None).get(); // TODO Port to coroutine

            // Write the logo buffer to the QIODevice
            iconSource->write(buffer.constData(), buffer.size());
            np.setPayload(iconSource, iconSource->size());
        }
    }

    m_plugin->sendPacket(np);
}
