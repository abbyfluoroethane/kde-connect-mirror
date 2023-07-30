/**
 * SPDX-FileCopyrightText: 2018 Richard Liebscher <richard.liebscher@gmail.com>
 * SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "notificationslistener.h"

class NotificationsListenerThread;

class DBusNotificationsListener : public NotificationsListener
{
    Q_OBJECT

public:
    explicit DBusNotificationsListener(KdeConnectPlugin *aPlugin);
    ~DBusNotificationsListener() override;

private:
    void onNotify(const QString &, uint, const QString &, const QString &, const QString &, const QStringList &, const QVariantMap &, int);

    bool parseImageDataArgument(const QVariant &argument,
                                int &width,
                                int &height,
                                int &rowStride,
                                int &bitsPerSample,
                                int &channels,
                                bool &hasAlpha,
                                QByteArray &imageData) const;
    QSharedPointer<QIODevice> iconForImageData(const QVariant &argument) const;
    QSharedPointer<QIODevice> iconForIconName(const QString &iconName) const;
    QSharedPointer<QIODevice> pngFromImage() const;

    NotificationsListenerThread *m_thread = nullptr;
};
