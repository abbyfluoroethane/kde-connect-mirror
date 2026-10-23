/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kdeconnect 1.0

Kirigami.ScrollablePage {
    property string device

    Component.onCompleted: {
        allowRemote.checked = config.getBool("allowRemote", false)
    }

    Kirigami.FormLayout {
        KdeConnectPluginConfig {
            id: config
            deviceId: device
            pluginName: "kdeconnect_geolocation"
        }

        QQC2.CheckBox {
            id: allowRemote
            text: i18nd("kdeconnect-plugins", "Allow other computers on the network to access geolocation")
            focus: true
            onClicked: config.set("allowRemote", checked)
        }
    }
}
