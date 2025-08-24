/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect

Kirigami.Dialog {
    id: volumeDialog
    
    property string deviceId: ""
    property var dbusInterface: MobileVolumeDbusInterface { deviceId: volumeDialog.deviceId }
    property var sliders: ({})
    property var labels: ({})
    
    title: i18n("Mobile Volume Control")
    standardButtons: Kirigami.Dialog.Close
    
    width: 400
    height: 500
    implicitWidth: 400
    implicitHeight: 500
    
    onOpened: {
        requestStreamInfo();
    }
    
    function requestStreamInfo() {
        dbusInterface.sendVolumeRequest();
        console.log("Requested stream information from device");
    }
    
    function createVolumeControls(streamsJson) {
        console.log("Creating volume controls from JSON:", streamsJson);
        
        try {
            const streamsArray = JSON.parse(streamsJson);
            
            // Clear existing controls
            for (const streamId in sliders) {
                sliders[streamId].destroy();
                labels[streamId].destroy();
            }
            sliders = {};
            labels = {};
            
            // Remove loading label
            loadingLabel.visible = false;
            
            // Create controls for each stream
            for (const streamObj of streamsArray) {
                const streamId = streamObj.stream;
                const name = streamObj.name || getStreamName(streamId);
                const current = streamObj.current || 0;
                const max = streamObj.max || 100;
                
                console.log("Creating control for stream:", streamId, name, current, "/", max);
                
                // Create label
                const label = labelComponent.createObject(volumeContent, {
                    text: i18n("%1: %2/%3", name, current, max)
                });
                labels[streamId] = label;
                
                // Create slider
                const slider = sliderComponent.createObject(volumeContent, {
                    from: 0,
                    to: max,
                    value: current,
                    streamId: streamId,
                    streamName: name,
                    streamMax: max
                });
                sliders[streamId] = slider;
            }
        } catch (error) {
            console.warn("Invalid streams JSON format:", error);
        }
    }
    
    function updateSlider(stream, value) {
        if (sliders[stream] && labels[stream]) {
            const slider = sliders[stream];
            const label = labels[stream];
            
            // Block signals to prevent infinite loop
            slider.blockSignals = true;
            slider.value = value;
            slider.blockSignals = false;
            
            // Update label text
            const max = slider.to;
            const name = slider.streamName || getStreamName(stream);
            label.text = i18n("%1: %2/%3", name, value, max);
        }
    }
    
    function getStreamName(stream) {
        const streamNames = {
            0: i18n("Voice Call"),
            1: i18n("System"),
            2: i18n("Ringtone"),
            3: i18n("Media"),
            4: i18n("Alarm"),
            5: i18n("Notification"),
            8: i18n("DTMF"),
            10: i18n("Accessibility")
        };
        return streamNames[stream] || i18n("Unknown Stream");
    }
    
    ColumnLayout {
        id: volumeContent
        spacing: Kirigami.Units.smallSpacing
        
        Label {
            id: loadingLabel
            text: i18n("Loading volume information...")
            visible: true
        }
    }
    
    Component {
        id: labelComponent
        Label {
            Layout.fillWidth: true
        }
    }
    
    Component {
        id: sliderComponent
        Slider {
            property int streamId: -1
            property string streamName: ""
            property int streamMax: 100
            property bool blockSignals: false
            
            Layout.fillWidth: true
            
            onValueChanged: {
                if (!blockSignals && streamId !== -1) {
                    console.log("Slider changed - Stream:", streamId, "Value:", value);
                    dbusInterface.sendVolumeChange(streamId, value);
                    
                    if (labels[streamId]) {
                        labels[streamId].text = i18n("%1: %2/%3", streamName, value, streamMax);
                    }
                }
            }
        }
    }
    
    Connections {
        target: dbusInterface
        function onVolumeChanged(stream, value) {
            updateSlider(stream, value);
        }
        function onStreamsInfoReceived(streamsJson) {
            createVolumeControls(streamsJson);
        }
    }
}