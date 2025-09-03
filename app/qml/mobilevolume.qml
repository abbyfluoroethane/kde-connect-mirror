/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
 
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect

Kirigami.ScrollablePage {
    id: root
    property QtObject device
    title: i18nd("kdeconnect-app", "Mobile Volume Control")

    property var mobileVolumeIface: MobileVolumeDbusInterfaceFactory.create(device.id())

    Component.onCompleted: {
        if (mobileVolumeIface) {
            mobileVolumeIface.sendVolumeRequest()
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Repeater {
            id: volumeRepeater
            model: ListModel {}

            delegate: ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: model.name
                    Layout.fillWidth: true
                    font.bold: true
                }

                QQC2.Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: model.max
                    value: model.current
                    onMoved: {
                        if (mobileVolumeIface) {
                            mobileVolumeIface.sendVolumeChange(model.stream, value)
                        }
                    }
                }

                QQC2.Label {
                    text: i18nd("kdeconnect-app", "Volume: %1/%2", Math.round(volumeSlider.value), Math.round(volumeSlider.to))
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                }
            }
        }

        QQC2.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: volumeRepeater.count === 0
            running: visible
        }
    }

    Connections {
        target: mobileVolumeIface
        enabled: mobileVolumeIface !== null
        
        function onStreamsInfoReceived(streamsJson) {
            try {
//                console.log("Received streams info:", streamsJson)
                var streams = JSON.parse(streamsJson)
                volumeRepeater.model.clear()
                
                for (var i = 0; i < streams.length; i++) {
                    var streamInfo = streams[i]
//                    console.log("Stream info:", streamInfo.stream, streamInfo)
                    
                    volumeRepeater.model.append({
                        stream: streamInfo.stream,
                        name: streamInfo.name,
                        current: streamInfo.current,
                        max: streamInfo.max
                    })
                }
            } catch (e) {
                console.error("Failed to parse streams JSON:", e, streamsJson)
            }
        }

        function onVolumeChanged(stream, volume) {
//            console.log("Volume changed - Stream:", stream, "Volume:", volume)
            for (var i = 0; i < volumeRepeater.model.count; i++) {
                if (volumeRepeater.model.get(i).stream === stream) {
                    volumeRepeater.model.setProperty(i, "current", volume)
                    break
                }
            }
        }
    }
}