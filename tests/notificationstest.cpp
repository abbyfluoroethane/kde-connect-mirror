/**
 * SPDX-FileCopyrightText: 2024 Kristen McWilliam <kmcwilliampublic@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kdeconnectplugin.h"
#include "testdaemon.h"

#include <KSystemClipboard>

#include <QJsonArray>
#include <QtTest>

/**
 * Test the NotificationsPlugin class
 */
class NotificationsPluginTest : public QObject
{
    Q_OBJECT

public:
    NotificationsPluginTest()
    {
        QStandardPaths::setTestModeEnabled(true);
        m_daemon = new TestDaemon;
    }

private:
    TestDaemon *m_daemon;

private Q_SLOTS:

    /**
     * If the user selects the action to copy an auth code, the auth code should
     * be copied to the desktop clipboard.
     */
    void testAuthCodeIsCopied()
    {
        const auto deviceIds = m_daemon->devices();
        Device *device = nullptr;
        for (const QString &deviceId : deviceIds) {
            Device *d = m_daemon->getDevice(deviceId);
            if (d->isReachable()) {
                if (!d->isPaired())
                    d->requestPairing();
                device = d;
            }
        }

        if (device == nullptr) {
            QFAIL("Unable to determine device");
        }

        QCOMPARE(device->isReachable(), true);
        QCOMPARE(device->isPaired(), true);

        KdeConnectPlugin *plugin = device->plugin(QStringLiteral("kdeconnect_notifications"));
        QVERIFY(plugin);

        const QString key = QStringLiteral("0|com.google.android.apps.messaging|2|com.google.android.apps.messaging:incoming_message:23|10129");

        // Note that the auth code we receive to work with has invisible
        // characters in it for some reason. (U+2063 INVISIBLE SEPARATOR between
        // each digit)
        //
        // `action` here is equal to `Copy \"6⁣7⁣1⁣7⁣3⁣3⁣\"`, but with the invisible
        // characters written out explicitly to make it easier to read, and to
        // prevent confusion when reading the test code.
        const QString action = QStringLiteral("Copy \"6\u20637\u20631\u20637\u20633\u20633\"");

        const QString expectedClipboardContents = QStringLiteral("671733");

        // Verify that the clipboard does not contain the auth code already
        const QString originalClipboardContents = KSystemClipboard::instance()->text(QClipboard::Clipboard);
        QVERIFY(originalClipboardContents != expectedClipboardContents);

        // Send the action
        plugin->metaObject()->invokeMethod(plugin, "sendAction", Q_ARG(QString, key), Q_ARG(QString, action));

        // Verify that the clipboard now contains the auth code
        const QString updatedClipboardContents = KSystemClipboard::instance()->text(QClipboard::Clipboard);
        QCOMPARE(updatedClipboardContents, expectedClipboardContents);

        // Set the clipboard back to its original state
        auto mimeData = new QMimeData;
        mimeData->setText(originalClipboardContents);
        KSystemClipboard::instance()->setMimeData(mimeData, QClipboard::Clipboard);
    }

    /**
     * The actions of a notification should be readable, so that clients can
     * offer them and trigger them with sendAction().
     */
    void testActionsAreExposed()
    {
        Device *device = nullptr;
        const auto deviceIds = m_daemon->devices();
        for (const QString &deviceId : deviceIds) {
            Device *d = m_daemon->getDevice(deviceId);
            if (d->isReachable()) {
                if (!d->isPaired())
                    d->requestPairing();
                device = d;
            }
        }
        QVERIFY(device);
        QVERIFY(device->isPaired());

        KdeConnectPlugin *plugin = device->plugin(QStringLiteral("kdeconnect_notifications"));
        QVERIFY(plugin);

        const QString key = QStringLiteral("0|org.example.messages|7|actions-test|10129");
        const QStringList actions = {QStringLiteral("Mark as read"), QStringLiteral("Call")};

        NetworkPacket np(QStringLiteral("kdeconnect.notification"),
                         {
                             {QStringLiteral("id"), key},
                             {QStringLiteral("appName"), QStringLiteral("Messages")},
                             {QStringLiteral("ticker"), QStringLiteral("Test")},
                             {QStringLiteral("silent"), true},
                             {QStringLiteral("actions"), QJsonArray::fromStringList(actions)},
                         });
        plugin->receivePacket(np);

        QObject *notification = nullptr;
        const auto children = plugin->findChildren<QObject *>();
        for (QObject *child : children) {
            if (child->property("internalId").toString() == key) {
                notification = child;
            }
        }
        QVERIFY(notification);
        QCOMPARE(notification->property("actions").toStringList(), actions);
    }
};

QTEST_MAIN(NotificationsPluginTest);

#include "notificationstest.moc"
