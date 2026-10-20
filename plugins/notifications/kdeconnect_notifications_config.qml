/**
 * SPDX-FileCopyrightText: 2026 Yuri Vidal <yurividal@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kdeconnect 1.0

Kirigami.ScrollablePage {
    property string device

    Kirigami.FormLayout {

        KdeConnectPluginConfig {
            id: config
            deviceId: device
            pluginName: "kdeconnect_notifications"
        }

        Component.onCompleted: {
            detectOtpCodes.checked = config.getBool("detectOtpCodes", true)
            autoCopyOtpCodes.checked = config.getBool("autoCopyOtpCodes", false)
            markReadOnCopy.checked = config.getBool("markReadOnCopy", true)
        }

        QQC2.CheckBox {
            id: detectOtpCodes
            text: i18nd("kdeconnect-plugins", "Detect verification codes and show a Copy button")
            focus: true
            onClicked: config.set("detectOtpCodes", checked)
            KeyNavigation.down: autoCopyOtpCodes
        }

        QQC2.CheckBox {
            id: autoCopyOtpCodes
            text: i18nd("kdeconnect-plugins", "Automatically copy detected codes to the clipboard")
            enabled: detectOtpCodes.checked
            onClicked: config.set("autoCopyOtpCodes", checked)
            KeyNavigation.down: markReadOnCopy
        }

        QQC2.CheckBox {
            id: markReadOnCopy
            text: i18nd("kdeconnect-plugins", "Mark the message as read when copying the code")
            enabled: detectOtpCodes.checked
            onClicked: config.set("markReadOnCopy", checked)
        }
    }
}
