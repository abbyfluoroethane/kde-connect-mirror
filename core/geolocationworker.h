/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef GEOLOCATIONWORKER_H
#define GEOLOCATIONWORKER_H

#include <QTcpServer>

#include "QObject"
#include "daemon.h"

class OrgFreedesktopAvahiEntryGroupInterface;
class OrgFreedesktopAvahiServer2Interface;


class GeolocationWorker : public QObject {
    Q_OBJECT
public:
    GeolocationWorker(Daemon *daemon = nullptr);
    ~GeolocationWorker() override;
    void chooseGpsProvider();

public Q_SLOTS:
    void slotNewConnection();
    void slotDisconnected(QTcpSocket* consumer);
    void forwardNmeaPacket(const QString &data);
Q_SIGNALS:
    void consumersChanged();

private:
    Daemon *m_daemon = nullptr;
    QList<QTcpSocket*> m_consumers;
    QTcpServer *m_localServer = nullptr;

    std::unique_ptr<OrgFreedesktopAvahiEntryGroupInterface> advertise();

    std::unique_ptr<OrgFreedesktopAvahiServer2Interface> m_avahiServerInterface;
    std::unique_ptr<OrgFreedesktopAvahiEntryGroupInterface> m_avahiEntryGroupInterface;
};


#endif //GEOLOCATIONWORKER_H
