/**
 * SPDX-FileCopyrightText: 2026 KDE Connect Developers
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kdeconnect 1.0
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root

    property string device
    readonly property string permissionKey: "remoteDesktopAccessCertificateSha256"
    property bool accessGranted: false

    function refreshPermission() {
        accessGranted = config.getString(permissionKey, "") !== ""
    }

    Component.onCompleted: refreshPermission()

    Kirigami.FormLayout {
        KdeConnectPluginConfig {
            id: config
            deviceId: root.device
            pluginName: "kdeconnect_virtualmonitor"
            onConfigChanged: root.refreshPermission()
        }

        Kirigami.InlineMessage {
            Kirigami.FormData.isSection: true
            Layout.fillWidth: true
            visible: root.accessGranted
            type: Kirigami.MessageType.Warning
            text: i18nd("kdeconnect-plugins", "This device is allowed to view the screen and control the mouse and keyboard.")
        }

        QQC2.Label {
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Remote desktop access:")
            text: root.accessGranted
                ? i18nd("kdeconnect-plugins", "Allowed")
                : i18nd("kdeconnect-plugins", "Ask on next connection")
        }

        QQC2.Button {
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Permission:")
            text: i18nd("kdeconnect-plugins", "Revoke Access")
            icon.name: "security-low"
            enabled: root.accessGranted
            onClicked: config.set(root.permissionKey, "")
        }

        QQC2.Label {
            Kirigami.FormData.isSection: true
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: i18nd("kdeconnect-plugins", "Access is granted only after confirmation on this computer and is bound to the paired device certificate. Revoking it also stops an active remote desktop session.")
        }
    }
}
