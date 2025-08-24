/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "mobilevolumedbusinterface.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDebug>

MobileVolumeDbusInterface::MobileVolumeDbusInterface(const QString &deviceId, QObject *parent)
    : QDBusAbstractInterface(QStringLiteral("org.kde.kdeconnect"),
                             QStringLiteral("/modules/kdeconnect/devices/") + deviceId + QStringLiteral("/mobilevolume"),
                             "org.kde.kdeconnect.device.mobilevolume",
                             QDBusConnection::sessionBus(),
                             parent)
{
    qDebug() << "MobileVolumeDbusInterface created for device:" << deviceId;
}

void MobileVolumeDbusInterface::sendVolumeChange(int stream, int value)
{
    qDebug() << "Sending volume change - Stream:" << stream << "Value:" << value;

    QDBusMessage reply = call(QStringLiteral("sendVolumeChange"), stream, value);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "DBus call failed:" << reply.errorName() << "-" << reply.errorMessage();
        qWarning() << "Failed to send volume change for stream" << stream << "to value" << value;
    } else {
        qDebug() << "Volume change sent successfully for stream" << stream;
    }
}

void MobileVolumeDbusInterface::sendVolumeRequest()
{
    QDBusMessage reply = call(QStringLiteral("sendVolumeRequest"));

    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "DBus call failed:" << reply.errorName() << "-" << reply.errorMessage();
        qWarning() << "Failed to send volume request";
    } else {
        qDebug() << "Volume request sent successfully";
    }
}