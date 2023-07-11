/**
 * SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "windowsnotificationslistener.h"

#include <optional>

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

void WindowsNotificationsListener::onNotificationChanged(const UserNotificationListener& sender, const UserNotificationChangedEventArgs& args)
{
    // Get the notification from the event arguments
    const UserNotificationChangedKind changeKind = args.ChangeKind();
    if (changeKind == UserNotificationChangedKind::Removed) {
        return;
    }

    IAsyncOperation<IVectorView<UserNotification>> notificationsAsyncOperation = sender.GetNotificationsAsync(NotificationKinds::Toast);
    // TODO use coroutine
    const unsigned notificationId = args.UserNotificationId();
    notificationsAsyncOperation.Completed([this, notificationId](const decltype(notificationsAsyncOperation)& operation, const AsyncStatus& status) {
        if (status != AsyncStatus::Completed) {
            return;
        }

        const IVectorView<UserNotification> notifications = operation.GetResults();
        std::optional<UserNotification> userNotification;
        for (unsigned i = 0; i < notifications.Size(); ++i) {
            if (notifications.GetAt(i).Id() == notificationId) {
                userNotification = notifications.GetAt(i);
                break;
            }
        }
        if (!userNotification.has_value()) {
            return;
        }

        const AppInfo appInfo = userNotification.value().AppInfo();
        const AppDisplayInfo appDisplayInfo = appInfo.DisplayInfo();
        const QString appName = QString::fromStdString(winrt::to_string(appDisplayInfo.DisplayName()));
        if (m_translatedAppName == appName) {
            return;
        }

        const winrt::Windows::UI::Notifications::Notification notification = userNotification.value().Notification();
        const NotificationBinding toastBinding = notification.Visual().GetBinding(KnownNotificationBindings::ToastGeneric());
        if (!toastBinding) {
            return;
        }
        const auto textElements = toastBinding.GetTextElements();
        std::wstring ticker;
        if (textElements.Size() == 0) {
            return;
        } else {
            const bool includeBody = m_plugin->config()->getBool(QStringLiteral("generalIncludeBody"), true);
            for (unsigned i = 0; i < (includeBody ? textElements.Size() : 1); ++i) {
                ticker += textElements.GetAt(i).Text() + L"\n";
            }
        }

        NetworkPacket np(PACKET_TYPE_NOTIFICATION,
            { { QStringLiteral("id"), userNotification.value().Id() },
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
    });
}
