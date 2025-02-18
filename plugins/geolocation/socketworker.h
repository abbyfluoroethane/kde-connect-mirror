/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QTcpServer>

#include "QObject"
#include "QLocalServer"

#ifndef SOCKETWORKER_H
#define SOCKETWORKER_H


class SocketWorker : public QObject {
    Q_OBJECT
public:
    SocketWorker(QObject *parent = nullptr);
    ~SocketWorker() override;

public Q_SLOTS:
    void process() const;
    void slotNewConnection();
    void slotDisconnected();
    void sendToCurrentClient(const QString &string) const;
Q_SIGNALS:
    void finished();
    void startedListening();
    void stoppedListening();
    void error(QString err);
private:
    // add your variables here
    QTcpServer *m_localServer;
    QTcpSocket *m_socket = nullptr;

    void sendToClient(QTcpSocket* localSocket, const QString& string) const;
};


#endif //SOCKETWORKER_H
