/*
 * SPDX-FileCopyrightText: 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect

ListView {
    id: devices

    focus: true

    signal clicked(string device)

    property string currentDeviceId

    section {
        property: "status"
        delegate: Kirigami.ListSectionHeader {

            width: ListView.view.width

            text: switch (parseInt(section))
            {
                case DevicesModel.Paired:
                    return i18nd("kdeconnect-kcm", "Remembered")
                case DevicesModel.Reachable:
                    return i18nd("kdeconnect-kcm", "Available")
                case (DevicesModel.Reachable | DevicesModel.Paired):
                    return i18nd("kdeconnect-kcm", "Connected")
            }
        }
    }
    Kirigami.PlaceholderMessage {
        text: i18nd("kdeconnect-kcm", "No devices found")
        anchors.centerIn: parent
        visible: devices.count === 0
    }

    delegate: ItemDelegate {
        id: delegate
        icon.name: iconName
        icon.color: "transparent"
        text: model.name
        width: ListView.view.width
        highlighted: devices.currentDeviceId === deviceId

        focus: true

        contentItem: Kirigami.IconTitleSubtitle {
            title: delegate.text
            subtitle: toolTip
            icon: icon.fromControlsIcon(delegate.icon)
        }

        onClicked: {
            devices.currentDeviceId = deviceId
            devices.clicked(deviceId)
        }
    }
}
