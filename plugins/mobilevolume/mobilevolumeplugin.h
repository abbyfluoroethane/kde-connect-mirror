/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef MOBILEVOLUMEPLUGIN_H
#define MOBILEVOLUMEPLUGIN_H

#include <QMap>
#include <QObject>
#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_MOBILEVOLUME QStringLiteral("kdeconnect.mobilevolume")
#define PACKET_TYPE_MOBILEVOLUME_REQUEST QStringLiteral("kdeconnect.mobilevolume.request")
#define PACKET_TYPE_MOBILEVOLUME_RESPONSE QStringLiteral("kdeconnect.mobilevolume.response")
#define PACKET_TYPE_MOBILEVOLUME_STREAMS QStringLiteral("kdeconnect.mobilevolume.streams")
#define PACKET_TYPE_MOBILEVOLUME_CHANGE QStringLiteral("kdeconnect.mobilevolume.change")

class MobileVolumePlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.mobilevolume")

public:
    explicit MobileVolumePlugin(QObject *parent, QVariantList args);
    ~MobileVolumePlugin();

    void receivePacket(const NetworkPacket &np) override;
    QString dbusPath() const override;

public Q_SLOTS:
    Q_SCRIPTABLE void sendVolumeChange(int stream, int value);
    Q_SCRIPTABLE void sendVolumeRequest();
    Q_SCRIPTABLE void emitCurrentVolumes();
    Q_SCRIPTABLE QString getStreamsInfo();

Q_SIGNALS:
    Q_SCRIPTABLE void volumeChanged(int stream, int volume);
    Q_SCRIPTABLE void streamsInfoReceived(const QString &streamsJson);

private:
    void handleVolumeResponse(const NetworkPacket &np);
    void handleStreamsInfo(const NetworkPacket &np);

private:
    QMap<int, int> currentVolumes;
    QMap<int, int> maxVolumes;
    QMap<int, QString> streamNames;
    QString m_lastStreamsJson;
};

#endif // MOBILEVOLUMEPLUGIN_H