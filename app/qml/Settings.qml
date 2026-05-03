// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import org.kde.kdeconnect
import org.kde.kdeconnect.app

Kirigami.ScrollablePage
{
    id: page
    title: i18nc("@title:window", "Settings")

    DBusProperty {
        id: announcedNameProperty
        object: DaemonDbusInterface
        read: "announcedName"
        defaultValue: ""
    }

    DBusProperty {
        id: linkProvidersProperty
        object: DaemonDbusInterface
        read: "linkProviders"
        defaultValue: []
    }

    ListModel {
        id: customDevicesModel
    }

    function syncCustomDevicesToDaemon() {
        var arr = [];
        for (var i = 0; i < customDevicesModel.count; i++) {
            arr.push(customDevicesModel.get(i).address);
        }
        DaemonDbusInterface.customDevices = arr;
    }

    function addCustomDevice(address) {
        var trimmed = address.trim();
        if (trimmed.length === 0) return;
        for (var i = 0; i < customDevicesModel.count; i++) {
            if (customDevicesModel.get(i).address === trimmed) return;
        }
        customDevicesModel.append({ "address": trimmed });
        syncCustomDevicesToDaemon();
    }

    function removeCustomDevice(index) {
        page.forceActiveFocus();
        customDevicesModel.remove(index);
        syncCustomDevicesToDaemon();
    }

    ColumnLayout
    {
        width: page.width

        FormCard.FormCard {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.gridUnit

            FormCard.FormTextFieldDelegate {
                id: deviceNameField
                text: announcedNameProperty.value
                onAccepted: DaemonDbusInterface.setAnnouncedName(text);
                label: i18n("Device name")
            }
        }

        FormCard.FormHeader {
            Layout.fillWidth: true
            title: i18nc("@title:group", "Backends")
        }

        FormCard.FormCard {
            Layout.fillWidth: true
            visible: linkProvidersProperty.value.length > 0

            Repeater {
                model: linkProvidersProperty.value

                FormCard.FormCheckDelegate {
                    required property string modelData

                    readonly property string displayName: modelData.split('|')[0]
                    readonly property string internalName: modelData.split('|')[1]

                    checked: modelData.split('|')[2] === 'enabled'
                    text: displayName

                    onToggled: DaemonDbusInterface.setLinkProviderState(internalName, checked);
                }
            }
        }

        FormCard.FormHeader {
            Layout.fillWidth: true
            title: i18nc("@title:group", "Custom Devices")
        }

        FormCard.FormCard {
            Layout.fillWidth: true

            Connections {
                target: DaemonDbusInterface
                function onCustomDevicesChanged() {
                    var val = DaemonDbusInterface.customDevices;
                    customDevicesModel.clear();
                    if (val) {
                        for (var i = 0; i < val.length; i++) {
                            customDevicesModel.append({ "address": val[i] });
                        }
                    }
                }
            }

            Component.onCompleted: {
                var val = DaemonDbusInterface.customDevices;
                customDevicesModel.clear();
                if (val) {
                    for (var i = 0; i < val.length; i++) {
                        customDevicesModel.append({ "address": val[i] });
                    }
                }
            }

            FormCard.FormTextDelegate {
                text: i18nc("@info", "Add devices by IP or hostname to connect directly over LAN or VPN.")
                visible: customDevicesModel.count === 0
            }

            Repeater {
                model: customDevicesModel

                delegate: FormCard.AbstractFormDelegate {
                    id: customDeviceDelegate
                    required property int index
                    required property string address

                    contentItem: RowLayout {
                        Kirigami.Icon {
                            source: "network-server"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        }
                        Label {
                            text: customDeviceDelegate.address
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        ToolButton {
                            icon.name: "edit-delete-remove"
                            text: i18nc("@action:button", "Remove")
                            display: AbstractButton.IconOnly
                            onClicked: Qt.callLater(removeCustomDevice, customDeviceDelegate.index)
                            ToolTip.text: text
                            ToolTip.visible: hovered
                            ToolTip.delay: Kirigami.Units.toolTipDelay
                        }
                    }
                }
            }

            FormCard.FormTextFieldDelegate {
                id: newDeviceField
                label: i18nc("@label:textbox", "Add device")
                placeholderText: i18nc("@placeholder", "IP or hostname (e.g. 100.64.0.2)")
                onAccepted: {
                    if (text.trim().length > 0) {
                        addCustomDevice(text);
                        text = "";
                    }
                }
            }

            FormCard.FormButtonDelegate {
                text: i18nc("@action:button", "Add")
                icon.name: "list-add"
                onClicked: {
                    if (newDeviceField.text.trim().length > 0) {
                        addCustomDevice(newDeviceField.text);
                        newDeviceField.text = "";
                    }
                }
            }
        }

        FormCard.FormCard {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.gridUnit

            FormCard.FormButtonDelegate {
                text: i18n("About KDE Connect")
                onClicked: applicationWindow().pageStack.layers.push(Qt.createComponent("org.kde.kirigamiaddons.formcard", "AboutPage"))
                icon.name: 'kdeconnect'
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormButtonDelegate {
                text: i18n("About KDE")
                onClicked: applicationWindow().pageStack.layers.push(Qt.createComponent("org.kde.kirigamiaddons.formcard", "AboutKDEPage"))
                icon.name: 'kde'
            }
        }
    }

    footer: ToolBar {
        contentItem: RowLayout {
            Item { Layout.fillWidth: true }
            Button {
                text: i18n("Close")
                display: AbstractButton.TextOnly
                onClicked: {
                    DaemonDbusInterface.setAnnouncedName(deviceNameField.text);
                    page.Kirigami.PageStack.closeDialog();
                }
            }
        }
    }
}
