/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <memory>

#include "advertise/nmeaadvertiser.h"

class QTcpServer;
class QTcpSocket;
class GeolocationPlugin;

class GeolocationWorker : public QObject
{
    Q_OBJECT
public:
    static GeolocationWorker *instance();
    ~GeolocationWorker() override;

    void registerPlugin(GeolocationPlugin *plugin);
    void unregisterPlugin(GeolocationPlugin *plugin);

    void forwardNmeaPacket(const QString &deviceId, const QString &data);

public Q_SLOTS:
    void chooseGpsProvider();
private Q_SLOTS:
    void slotNewConnection();
    void slotDisconnected(QTcpSocket *consumer);
Q_SIGNALS:
    void pluginsUpdated();
    void consumersUpdated();

private:
    explicit GeolocationWorker(QObject *parent = nullptr);

    void updateServerState();
    void dropConsumers(bool onlyRemote);

    QList<GeolocationPlugin *> m_plugins;
    QString m_activeProviderId;
    QList<QTcpSocket *> m_consumers;
    QTcpServer *m_localServer = nullptr;
    NmeaAdvertiser m_nmeaAdvertiser;
};
