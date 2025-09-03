/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef MOBILEVOLUMEDBUSINTERFACE_H
#define MOBILEVOLUMEDBUSINTERFACE_H

#include <QDBusAbstractInterface>
#include <QJsonArray>
#include <QObject>

class MobileVolumeDbusInterface : public QDBusAbstractInterface
{
    Q_OBJECT

public:
    explicit MobileVolumeDbusInterface(const QString &deviceId, QObject *parent = nullptr);

public Q_SLOTS:
    void sendVolumeChange(int stream, int value);
    void sendVolumeRequest();

private Q_SLOTS:
    void onVolumeChanged(int stream, int volume);
    void onStreamsInfoReceived(const QString &streamsJson);

Q_SIGNALS:
    void volumeChanged(int stream, int volume);
    void streamsInfoReceived(const QString &streamsJson);
};

#endif // MOBILEVOLUMEDBUSINTERFACE_H