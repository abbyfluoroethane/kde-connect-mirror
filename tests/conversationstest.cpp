/**
 * SPDX-FileCopyrightText: 2026 Jenser Almeida <jenseralmeida@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kdeconnectplugin.h"
#include "models/conversationmessage.h"
#include "testdaemon.h"

#include <QDBusAbstractAdaptor>
#include <QSignalSpy>
#include <QtTest>

/**
 * Test the conversations D-Bus interface of the SmsPlugin
 */
class ConversationsTest : public QObject
{
    Q_OBJECT

public:
    ConversationsTest()
    {
        QStandardPaths::setTestModeEnabled(true);
        m_daemon = new TestDaemon;
    }

private:
    TestDaemon *m_daemon;

    static QVariantMap message(qint32 uID, qint64 threadID, qint64 date, qint32 read)
    {
        return {
            {QStringLiteral("event"), ConversationMessage::EventTextMessage},
            {QStringLiteral("body"), QStringLiteral("Test")},
            {QStringLiteral("date"), date},
            {QStringLiteral("type"), ConversationMessage::MessageTypeInbox},
            {QStringLiteral("read"), read},
            {QStringLiteral("thread_id"), threadID},
            {QStringLiteral("_id"), uID},
            {QStringLiteral("addresses"), QVariantList{QVariantMap{{QStringLiteral("address"), QStringLiteral("5550100")}}}},
        };
    }

    static NetworkPacket messagesPacket(const QVariantList &messages)
    {
        return NetworkPacket(QStringLiteral("kdeconnect.sms.messages"), {{QStringLiteral("messages"), messages}});
    }

    static qint32 readStatus(QObject *conversations, qint64 threadID)
    {
        QVariantList active;
        QMetaObject::invokeMethod(conversations, "activeConversations", Q_RETURN_ARG(QVariantList, active));
        for (const QVariant &variant : std::as_const(active)) {
            const auto message = variant.value<ConversationMessage>();
            if (message.threadID() == threadID) {
                return message.read();
            }
        }
        return -1;
    }

private Q_SLOTS:

    /**
     * A message read on the phone is sent again with its new read status,
     * which should replace the cached copy and be announced.
     */
    void testReadStatusOfKnownMessageIsUpdated()
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

        KdeConnectPlugin *plugin = device->plugin(QStringLiteral("kdeconnect_sms"));
        QVERIFY(plugin);

        QObject *conversations = nullptr;
        const auto adaptors = device->findChildren<QDBusAbstractAdaptor *>();
        for (QDBusAbstractAdaptor *adaptor : adaptors) {
            if (qstrcmp(adaptor->metaObject()->className(), "ConversationsDbusInterface") == 0) {
                conversations = adaptor;
            }
        }
        QVERIFY(conversations);

        const qint64 threadID = 42;
        plugin->receivePacket(messagesPacket({message(1, threadID, 1000, 0), message(2, threadID, 2000, 0)}));
        QCOMPARE(readStatus(conversations, threadID), 0);

        QSignalSpy updated(conversations, SIGNAL(conversationUpdated(QDBusVariant)));

        // The latest message was read on the phone
        plugin->receivePacket(messagesPacket({message(2, threadID, 2000, 1)}));
        QCOMPARE(readStatus(conversations, threadID), 1);
        QCOMPARE(updated.count(), 1);

        // Receiving it again unchanged is not an update
        plugin->receivePacket(messagesPacket({message(2, threadID, 2000, 1)}));
        QCOMPARE(updated.count(), 1);

        // An older message changing is stored, but it is not the conversation's latest
        plugin->receivePacket(messagesPacket({message(1, threadID, 1000, 1)}));
        QCOMPARE(updated.count(), 1);
    }
};

QTEST_MAIN(ConversationsTest);

#include "conversationstest.moc"
