/*
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect

Kirigami.Page
{
    id: mousepad
    title: i18nd("kdeconnect-app", "Remote Control")
    property QtObject pluginInterface
    property QtObject device

    // Otherwise swiping on the MouseArea might trigger changing the page
    Kirigami.ColumnView.preventStealing: true

    Component.onCompleted: {
        PointerLocker.window = applicationWindow()
    }

    ColumnLayout
    {
        anchors.fill: parent

        MouseArea {
            id: area
            Layout.fillWidth: true
            Layout.fillHeight: true
            property var lastPos: Qt.point(-1, -1)
            property var pressedPos: Qt.point(-1, -1)
            property var releasedPos: Qt.point(-1, -1)

            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

            Button {
                id: lockButton
                anchors.centerIn: parent
                text: i18n("Lock")
                visible: !Kirigami.Settings.tabletMode && !PointerLocker.isLocked
                onClicked: {
                    PointerLocker.isLocked = true
                    area.pressedPos = Qt.point(-1, -1);
                }
            }
            Label {
                anchors.centerIn: parent
                visible: PointerLocker.isLocked
                width: parent.width

                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter

                text: i18n("Press %1 or the left and right mouse buttons at the same time to unlock", unlockShortcut.nativeText)
            }

            Connections {
                target: PointerLocker
                function onPointerMoved(delta) {
                    if (!PointerLocker.isLocked) {
                        return;
                    }
                    mousepad.pluginInterface.moveCursor(Qt.point(delta.x, delta.y));
                }
            }

            MultiPointTouchArea {
                id: touchArea
                anchors.fill: parent
                mouseEnabled: false

                touchPoints: [
                    TouchPoint { id: touch1 }
                ]

                property var tapCount: 0
                property var ignoreTaps: false
                property var holdTap: false

                // This is the maximum time between the first touch and release for the tap/click events
                Timer {
                    id: tapTimer
                    interval: 100
                    onTriggered: touchArea.ignoreTaps = true;
                }

                // This is the minimum time for the tap-and-hold event to trigger
                Timer {
                    id: holdTimer
                    interval: 500
                    onTriggered: {
                        if (touch1.x != touch1.startX || touch1.y != touch1.startY) {
                            return;
                        }

                        touchArea.holdTap = true;
                        mousepad.pluginInterface.sendCommand({"singlehold" : true});
                    }
                }

                onPressed: (points) => {
                    // We don't want to see the lock button when using a touchscreen
                    lockButton.visible = false;

                    if (tapCount === 0) {
                        tapTimer.running = true;
                    }

                    tapCount += points.length;

                    if (!holdTap) {
                        holdTimer.running = tapCount === 1;
                    }
                }

                function subtractTapCount(n: int) {
                    tapCount -= n;
                    if (tapCount == 0) {
                        ignoreTaps = false;

                        if (holdTap) {
                            holdTap = false;
                            mousepad.pluginInterface.sendCommand({"singlerelease" : true});
                        }
                    }
                }

                onReleased: (points) => {
                    if (ignoreTaps || holdTap) {
                        subtractTapCount(points.length);
                        return;
                    }

                    tapTimer.running = false;
                    ignoreTaps = true; // Avoid sending multiple click events if the user did a multi finger tap but lifted some of the fingers slower than the others

                    var clickType = "";
                    var packet = {};

                    switch (tapCount) {
                    case 1:
                        clickType = "singleclick";
                        break;
                    case 2:
                        clickType = "rightclick";
                        break;
                    case 3:
                        clickType = "middleclick";
                        break;
                    }

                    if (clickType) {
                        packet[clickType] = true;
                        mousepad.pluginInterface.sendCommand(packet);
                    }

                    subtractTapCount(points.length);
                }

                onTouchUpdated: (points) => {
                    switch (points.length) {
                    case 1:
                        var deltaX = points[0].x - points[0].previousX;
                        var deltaY = points[0].y - points[0].previousY;
                        var delta = Qt.point(deltaX * 2, deltaY * 2);
                        pluginInterface.moveCursor(delta);
                        break;
                    case 2:
                        var prevX = (points[0].previousX + points[1].previousX) / 2
                        var prevY = (points[0].previousY + points[1].previousY) / 2
                        var newX = (points[0].x + points[1].x) / 2
                        var newY = (points[0].y + points[1].y) / 2
                        delta = Qt.point(newX - prevX, newY - prevY);
                        var packet = {};
                        packet["scroll"] = true;
                        packet["dy"] = newY - prevY;
                        packet["dx"] = newX - prevX;
                        mousepad.pluginInterface.sendCommand(packet);
                        break;
                    }
                }
            }

            Shortcut {
                id: unlockShortcut
                sequence: "Alt+X"
                onActivated: {
                    console.log("shortcut triggered, unlocking")
                    PointerLocker.isLocked = false
                }
            }

            onClicked: {
                var clickType = "";
                var packet = {};
                switch (mouse.button) {
                case Qt.LeftButton:
                    if (PointerLocker.isLocked) // we directly send singleclick and singlehold to emulate all mouse actions inc. drag and drop
                        return;
                    if (pressedPos == releasedPos) {
                        clickType = "singleclick";
                    }
                    break;
                case Qt.RightButton:
                    clickType = "rightclick";
                    break;
                case Qt.MiddleButton:
                    clickType = "middleclick";
                    break;
                default:
                    console.log("This click input is not handled yet!");
                    break;
                }

                if (clickType) {
                    packet[clickType] = true;
                    mousepad.pluginInterface.sendCommand(packet);
                }
            }

            onPressAndHold: {
                if (PointerLocker.isLocked)
                    return;                     // we send singlehold and singlerelease twice instead through onPressed and onReleased
                var clickType = "";
                var packet = {};
                switch (mouse.button) {
                case Qt.LeftButton:
                    clickType = "singlehold";
                    break;
                default:
                    console.log("This click input is not handled yet!");
                    break;
                }

                if (clickType) {
                    packet[clickType] = true;
                    mousepad.pluginInterface.sendCommand(packet);
                }
            }

            onPositionChanged: {
                if (lastPos.x > -1) {
    //                 console.log("move", mouse.x, mouse.y, lastPos)
                    var delta = Qt.point(mouse.x-lastPos.x, mouse.y-lastPos.y);

                    pluginInterface.moveCursor(delta);
                }
                lastPos = Qt.point(mouse.x, mouse.y);
            }

            Keys.onPressed: {
                if (event.key == Qt.Key_X) {
                    PointerLocker.isLocked = false
                    event.accepted = true;
                }
            }
            onPressed: {
                if (PointerLocker.isLocked) {
                    if (pressedButtons === (Qt.LeftButton | Qt.RightButton)) {
                        PointerLocker.isLocked = false
                    }
                    var pressType = "";
                    var packet = {};
                    switch (mouse.buttons) {
                    case Qt.LeftButton:
                        pressType = "singlehold";
                        break;
                    default:
                        console.log("This click input is not handled yet!");
                        break;
                    }
                    if (pressType) {
                        packet[pressType] = true;
                        mousepad.pluginInterface.sendCommand(packet);
                    }
                } else {
                    pressedPos = Qt.point(mouse.x, mouse.y);
                }
            }

            onWheel: {
                var packet = {};
                packet["scroll"] = true;
                packet["dy"] = wheel.angleDelta.y;
                packet["dx"] = wheel.angleDelta.x;
                mousepad.pluginInterface.sendCommand(packet);
            }

            onReleased: {
                if (!PointerLocker.isLocked) {
                    lastPos = Qt.point(-1,-1);
                    releasedPos = Qt.point(mouse.x, mouse.y);
                }
                mousepad.pluginInterface.sendCommand({"singlerelease" : true});
            }
        }

        RemoteKeyboard {
            device: mousepad.device
            Layout.fillWidth: true
            visible: remoteState
            focus: !lockButton.visible
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                Layout.fillWidth: true
                icon.name: "input-mouse-click-left"
                onClicked: mousepad.pluginInterface.sendCommand({"singleclick": true});
            }
            Button {
                Layout.fillWidth: true
                icon.name: "input-mouse-click-middle"
                onClicked: mousepad.pluginInterface.sendCommand({"middleclick": true});
            }
            Button {
                Layout.fillWidth: true
                icon.name: "input-mouse-click-right"
                onClicked: mousepad.pluginInterface.sendCommand({"rightclick": true});
            }
        }
    }
}
