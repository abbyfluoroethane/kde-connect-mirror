/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <memory>

class QTcpServer;
class QTcpSocket;
class OrgFreedesktopAvahiEntryGroupInterface;
class OrgFreedesktopAvahiServer2Interface;
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

private:
    explicit GeolocationWorker(QObject *parent = nullptr);

    std::unique_ptr<OrgFreedesktopAvahiEntryGroupInterface> advertise();

    QList<GeolocationPlugin *> m_plugins;
    QString m_activeProviderId;
    QList<QTcpSocket *> m_consumers;
    QTcpServer *m_localServer = nullptr;

    std::unique_ptr<OrgFreedesktopAvahiServer2Interface> m_avahiServerInterface;
    std::unique_ptr<OrgFreedesktopAvahiEntryGroupInterface> m_avahiEntryGroupInterface;
};
