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

    ColumnLayout
    {

        FormCard.FormCard {
            Layout.topMargin: Kirigami.Units.gridUnit

            FormCard.FormTextFieldDelegate {
                id: deviceNameField
                text: announcedNameProperty.value
                onAccepted: DaemonDbusInterface.setAnnouncedName(text);
                label: i18n("Device name")

                DBusProperty {
                    id: announcedNameProperty
                    object: DaemonDbusInterface
                    read: "announcedName"
                    defaultValue: ""
                }
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Backends")
        }

        FormCard.FormCard {
            DBusProperty {
                id: linkProvidersProperty
                object: DaemonDbusInterface
                read: "linkProviders"
                defaultValue: []
            }
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
            title: i18nc("@title:group", "Geolocation")
            visible: gpsProvidersProperty.value.length > 0
        }

        FormCard.FormCard {
            DBusProperty {
                id: gpsProvidersProperty
                object: DaemonDbusInterface
                read: "gpsProviders"
                defaultValue: []
            }
            DBusProperty {
                id: preferredGpsProviderProperty
                object: DaemonDbusInterface
                read: "preferredGpsProvider"
                defaultValue: ""
            }
            DBusProperty {
                id: gpsAllowRemoteProperty
                object: DaemonDbusInterface
                read: "gpsAllowRemote"
                defaultValue: false
            }
            visible: gpsProvidersProperty.value.length > 0

            FormCard.FormCheckDelegate {
                text: i18nc("@option:check", "Allow other computers on the network to access geolocation")
                checked: gpsAllowRemoteProperty.value
                onToggled: DaemonDbusInterface.setGpsAllowRemote(checked);
            }

            FormCard.FormComboBoxDelegate {
                text: i18nc("@title:group", "Preferred GPS provider")
                model: {
                    let rawList = gpsProvidersProperty.value || [];
                    return rawList.map(item => {
                        let parts = item.split('|');
                        return {
                            deviceId: parts[0] || "",
                            displayName: parts[1] || ""
                        };
                    });
                }

                textRole: "displayName"
                valueRole: "deviceId"

                readonly property string preferredId: preferredGpsProviderProperty.value

                function selectPreferred() {
                    currentIndex = indexOfValue(preferredId);
                }

                onPreferredIdChanged: selectPreferred()
                onModelChanged: Qt.callLater(selectPreferred)

                onActivated: index => DaemonDbusInterface.setPreferredGpsProvider(valueAt(index))
            }
        }

        FormCard.FormCard {
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
