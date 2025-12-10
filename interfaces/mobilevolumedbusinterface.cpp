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

    bool connected = QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.kdeconnect"),
                                                           QStringLiteral("/modules/kdeconnect/devices/") + deviceId + QStringLiteral("/mobilevolume"),
                                                           QStringLiteral("org.kde.kdeconnect.device.mobilevolume"),
                                                           QStringLiteral("volumeChanged"),
                                                           this,
                                                           SLOT(onVolumeChanged(int, int)));

    if (!connected) {
        qWarning() << "Failed to connect to volumeChanged signal";
    }

    connected = QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.kdeconnect"),
                                                      QStringLiteral("/modules/kdeconnect/devices/") + deviceId + QStringLiteral("/mobilevolume"),
                                                      QStringLiteral("org.kde.kdeconnect.device.mobilevolume"),
                                                      QStringLiteral("streamsInfoReceived"),
                                                      this,
                                                      SLOT(onStreamsInfoReceived(QString)));

    if (!connected) {
        qWarning() << "Failed to connect to streamsInfoReceived signal";
    }
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

void MobileVolumeDbusInterface::onVolumeChanged(int stream, int volume)
{
    // qDebug() << "Received volumeChanged signal from DBus:" << stream << volume;
    Q_EMIT volumeChanged(stream, volume);
}

void MobileVolumeDbusInterface::onStreamsInfoReceived(const QString &streamsJson)
{
    // qDebug() << "Received streamsInfoReceived signal from DBus:" << streamsJson;
    Q_EMIT streamsInfoReceived(streamsJson);
}