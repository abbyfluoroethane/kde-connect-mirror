/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */


#include "socketworker.h"
#include "QLocalSocket"
#include "QDir"
#include "QLocalServer"
#include <QTcpServer>
#include <QTcpSocket>

#include "plugin_geolocation_debug.h"

SocketWorker::SocketWorker(QObject *) {
    m_localServer = new QTcpServer(this);
    m_localServer->listen(QHostAddress::Any, 10110);
}
SocketWorker::~SocketWorker() {
    qCInfo(KDECONNECT_PLUGIN_GEOLOCATION) << "Destroying NMEA socket";
    m_localServer->close();
}

void SocketWorker::process() const {

    connect(m_localServer, &QTcpServer::newConnection, this, &SocketWorker::slotNewConnection);
}

void SocketWorker::slotNewConnection() {
    qCInfo(KDECONNECT_PLUGIN_GEOLOCATION) << "Client connected to NMEA socket.";
    if (m_socket != nullptr && m_socket->isOpen()) {
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected();
        m_socket->close();
    }
    if (m_localServer->hasPendingConnections())
    {
        m_socket = m_localServer->nextPendingConnection();
        connect(m_socket, &QTcpSocket::disconnected, this, &SocketWorker::slotDisconnected);
        this->startedListening();
    }
}

void SocketWorker::sendToCurrentClient(const QString &string) const {
    if (m_socket != nullptr && m_socket->isWritable()) {
        sendToClient(m_socket, string);
    }
}

void SocketWorker::slotDisconnected() {
    if (m_socket != nullptr) {
        disconnect(m_socket, &QTcpSocket::disconnected, this, &SocketWorker::slotDisconnected);
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    this->stoppedListening();
    qCInfo(KDECONNECT_PLUGIN_GEOLOCATION) << "Client closed connection to NMEA socket.";
}

void SocketWorker::sendToClient(QTcpSocket* localSocket, const QString& string) const
{
    localSocket->write(string.toLatin1());
}