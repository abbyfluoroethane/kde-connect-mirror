/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "mobilevolumeplugin.h"

#include <KPluginFactory>
#include <QDBusConnection>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "plugin_mobilevolume_debug.h"

K_PLUGIN_CLASS_WITH_JSON(MobileVolumePlugin, "kdeconnect_mobilevolume.json")

MobileVolumePlugin::MobileVolumePlugin(QObject *parent, QVariantList args)
    : KdeConnectPlugin(parent, args)
{
    streamNames[0] = QStringLiteral("Voice Call"); // STREAM_VOICE_CALL
    streamNames[1] = QStringLiteral("System"); // STREAM_SYSTEM
    streamNames[2] = QStringLiteral("Ringtone"); // STREAM_RING
    streamNames[3] = QStringLiteral("Media"); // STREAM_MUSIC
    streamNames[4] = QStringLiteral("Alarm"); // STREAM_ALARM
    streamNames[5] = QStringLiteral("Notification"); // STREAM_NOTIFICATION
    // streamNames[8] = QStringLiteral("DTMF"); // STREAM_DTMF
    // streamNames[10] = QStringLiteral("Accessibility"); // STREAM_ACCESSIBILITY

    QString path = dbusPath();
    bool registered =
        QDBusConnection::sessionBus().registerObject(path, this, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);

    if (registered) {
        qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Registered MobileVolume plugin on DBus at:" << path;
    } else {
        qCWarning(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Failed to register MobileVolume plugin on DBus at:" << path;
    }

    connect(this, &KdeConnectPlugin::connected, this, [this]() {
        sendVolumeRequest();
    });
}

MobileVolumePlugin::~MobileVolumePlugin()
{
    // Unregister DBus
    QDBusConnection::sessionBus().unregisterObject(dbusPath());
}

QString MobileVolumePlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/mobilevolume").arg(device()->id());
}

void MobileVolumePlugin::receivePacket(const NetworkPacket &np)
{
    qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "=== MOBILE VOLUME PACKET RECEIVED ===";
    qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Type:" << np.type();
    qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "====================================";

    if (np.type() == PACKET_TYPE_MOBILEVOLUME_RESPONSE) {
        qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Handling volume response (simple format)";
        handleVolumeResponse(np);
    } else if (np.type() == PACKET_TYPE_MOBILEVOLUME_STREAMS) {
        qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Handling streams info (detailed format)";
        handleStreamsInfo(np);
    } else if (np.type() == PACKET_TYPE_MOBILEVOLUME_REQUEST) {
        qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Received volume request from Android";
    } else {
        qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Unknown packet type, ignoring";
    }
}

void MobileVolumePlugin::sendVolumeRequest()
{
    // Send request to Android to get current volumes
    NetworkPacket np(PACKET_TYPE_MOBILEVOLUME_REQUEST);
    sendPacket(np);
    qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Sent volume request to device";
}

void MobileVolumePlugin::handleVolumeResponse(const NetworkPacket &np)
{
    if (!np.has(QStringLiteral("volumes"))) {
        qCWarning(KDECONNECT_PLUGIN_MOBILEVOLUME) << "No volumes field in response packet";
        return;
    }

    QString volumesStr = np.get<QString>(QStringLiteral("volumes"));
    QJsonDocument doc = QJsonDocument::fromJson(volumesStr.toUtf8());

    if (!doc.isObject()) {
        qCWarning(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Invalid volumes JSON format";
        return;
    }

    QJsonObject volumesObj = doc.object();
    for (auto it = volumesObj.begin(); it != volumesObj.end(); ++it) {
        QString streamName = it.key();
        int volume = it.value().toInt();

        // Find stream ID by name
        int streamId = -1;
        for (auto streamIt = streamNames.begin(); streamIt != streamNames.end(); ++streamIt) {
            if (streamIt.value() == streamName) {
                streamId = streamIt.key();
                break;
            }
        }

        if (streamId != -1) {
            currentVolumes[streamId] = volume;
            Q_EMIT volumeChanged(streamId, volume);
            qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Volume updated - Stream:" << streamId << streamName << "Volume:" << volume;
        } else {
            qCWarning(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Unknown stream name:" << streamName;
        }
    }
}

void MobileVolumePlugin::handleStreamsInfo(const NetworkPacket &np)
{
    if (!np.has(QStringLiteral("streams"))) {
        qCWarning(KDECONNECT_PLUGIN_MOBILEVOLUME) << "No streams field in packet";
        return;
    }

    QString streamsStr = np.get<QString>(QStringLiteral("streams"));
    m_lastStreamsJson = streamsStr; // Store the JSON string

    QJsonDocument doc = QJsonDocument::fromJson(streamsStr.toUtf8());

    if (!doc.isArray()) {
        qCWarning(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Invalid streams JSON format";
        return;
    }

    QJsonArray streamsArray = doc.array();

    // Emit signal with streams data as JSON string
    Q_EMIT streamsInfoReceived(streamsStr);

    for (const auto &streamValue : streamsArray) {
        QJsonObject streamObj = streamValue.toObject();
        int streamId = streamObj.value(QStringLiteral("stream")).toInt();
        QString name = streamObj.value(QStringLiteral("name")).toString();
        int current = streamObj.value(QStringLiteral("current")).toInt();
        int max = streamObj.value(QStringLiteral("max")).toInt();

        currentVolumes[streamId] = current;
        maxVolumes[streamId] = max;
        streamNames[streamId] = name;

        qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Stream info - ID:" << streamId << "Name:" << name << "Current:" << current << "Max:" << max;

        Q_EMIT volumeChanged(streamId, current);
    }
}

void MobileVolumePlugin::sendVolumeChange(int stream, int value)
{
    NetworkPacket np(PACKET_TYPE_MOBILEVOLUME_CHANGE);
    np.set(QStringLiteral("stream"), stream);
    np.set(QStringLiteral("volume"), value);
    sendPacket(np);

    currentVolumes[stream] = value;
    Q_EMIT volumeChanged(stream, value);

    qCDebug(KDECONNECT_PLUGIN_MOBILEVOLUME) << "Sent volume change - Stream:" << stream << "Value:" << value;
}

void MobileVolumePlugin::emitCurrentVolumes()
{
    for (auto it = currentVolumes.begin(); it != currentVolumes.end(); ++it) {
        Q_EMIT volumeChanged(it.key(), it.value());
    }
}

QString MobileVolumePlugin::getStreamsInfo()
{
    return m_lastStreamsJson;
}

#include "mobilevolumeplugin.moc"