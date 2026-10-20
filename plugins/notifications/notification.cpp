/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "notification.h"
#include "otpdetector.h"
#include "plugin_notifications_debug.h"

#include <KLocalizedString>
#include <KNotification>
#include <KNotificationReplyAction>
#include <KSystemClipboard>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMimeData>
#include <QPixmap>
#include <QRegularExpression>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>
#include <knotifications_version.h>

#include <core/filetransferjob.h>
#include <core/notificationserverinfo.h>

QMap<QString, FileTransferJob *> Notification::s_downloadsInProgress;

namespace
{
void copyCodeToClipboard(const QString &code)
{
    auto mimeData = new QMimeData;
    mimeData->setText(code);
    // Keep the code out of the clipboard manager's history
    mimeData->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
    KSystemClipboard::instance()->setMimeData(mimeData, QClipboard::Clipboard);
}

QString digitsOnly(const QString &text)
{
    QString result;
    for (const QChar c : text) {
        if (c.isDigit()) {
            result.append(c);
        }
    }
    return result;
}
}

Notification::Notification(const NetworkPacket &np, const Device *device, KdeConnectPluginConfig *config, QObject *parent)
    : QObject(parent)
    , m_imagesDir()
    , m_device(device)
    , m_config(config)
{
    // Make a own directory for each user so no one can see each others icons
    QString username;
#ifdef Q_OS_WIN
    username = QString::fromLatin1(qgetenv("USERNAME"));
#else
    username = QString::fromLatin1(qgetenv("USER"));
#endif

    m_imagesDir.setPath(QDir::temp().absoluteFilePath(QStringLiteral("kdeconnect_") + username));
    m_imagesDir.mkpath(m_imagesDir.absolutePath());
    QFile(m_imagesDir.absolutePath()).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    m_ready = false;

    parseNetworkPacket(np);
    createKNotification(np);
}

void Notification::dismiss()
{
    if (m_dismissable) {
        Q_EMIT dismissRequested(m_internalId);
    }
}

void Notification::show()
{
    m_ready = true;
    Q_EMIT ready();
    if (!m_silent && m_notification) {
        m_notification->sendEvent();
    }
}

void Notification::update(const NetworkPacket &np)
{
    const QString previousCode = m_otpCode;
    parseNetworkPacket(np);

    // A new message in an unread conversation arrives as a silent update of the same
    // notification. If it carries a new code, alert anyway so it is noticed.
    if (m_silent && !otpCode().isEmpty() && m_otpCode != previousCode) {
        m_silent = false;
    }

    createKNotification(np);
}

void Notification::createKNotification(const NetworkPacket &np)
{
    if (!m_notification) {
        m_notification = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout, this);
        m_notification->setComponentName(QStringLiteral("kdeconnect"));
        m_notification->setHint(QStringLiteral("resident"),
                                true); // This means the notification won't be deleted automatically, but only with KNotifications 5.81
    }

    const bool supportsMarkup = NotificationServerInfo::instance().supportedHints().testFlag(NotificationServerInfo::BODY_MARKUP);

    QString ticker = m_ticker;
    QString title = m_title;
    QString text = isConversation() ? getConversationMessages(supportsMarkup) : m_text;

    if (supportsMarkup) {
        ticker = ticker.toHtmlEscaped();
        title = title.toHtmlEscaped();
        if (!isConversation()) {
            // getConversationMessages() already produces escaped text for conversations
            text = text.toHtmlEscaped();
        }
    }

    if (NotificationServerInfo::instance().supportedHints().testFlag(NotificationServerInfo::X_KDE_DISPLAY_APPNAME)) {
        m_notification->setTitle(m_title);
        m_notification->setText(text);
        m_notification->setHint(QStringLiteral("x-kde-display-appname"), m_appName);
    } else {
        m_notification->setTitle(m_appName);

        if (title.isEmpty() && text.isEmpty()) {
            m_notification->setText(ticker);
        } else if (m_appName == m_title || title.isEmpty()) {
            m_notification->setText(text);
        } else if (text.isEmpty()) {
            m_notification->setText(title);
        } else {
            m_notification->setText(title + QStringLiteral(": ") + text);
        }
    }

    const QString otpCode = this->otpCode();
    if (!otpCode.isEmpty()) {
        // Silent notifications are resent on reconnect, don't overwrite the clipboard for those
        if (!m_silent && otpCode != m_lastAutoCopiedCode && m_config->getBool(QStringLiteral("autoCopyOtpCodes"), false)) {
            copyCodeToClipboard(otpCode);
            m_lastAutoCopiedCode = otpCode;
        }
        if (otpCode == m_lastAutoCopiedCode) {
            const QString separator = supportsMarkup ? QStringLiteral("<br/>") : QStringLiteral("\n");
            m_notification->setText(m_notification->text() + separator + i18nc("@info %1 is a verification code", "Code %1 copied to clipboard", otpCode));
        }
    }

    m_notification->setHint(QStringLiteral("x-kde-origin-name"), m_device->name());
    // Mark this notification so sendnotifications can filter it out and not echo it back to the device
    m_notification->setHint(QStringLiteral("x-kdeconnect-source-device"), m_device->name());

    if (!m_requestReplyId.isEmpty()) {
        auto replyAction = std::make_unique<KNotificationReplyAction>(i18nc("@action:button", "Reply"));

        if (isGroupConversation()) {
            replyAction->setPlaceholderText(i18nc("@info:placeholder %1 is the name of a group chat", "Reply in %1…", m_groupName));
        } else {
            replyAction->setPlaceholderText(i18nc("@info:placeholder", "Reply to %1…", isConversation() ? m_title : m_appName));
        }

        replyAction->setFallbackBehavior(KNotificationReplyAction::FallbackBehavior::UseRegularAction);
        QObject::connect(replyAction.get(), &KNotificationReplyAction::replied, this, &Notification::replied);
        QObject::connect(replyAction.get(), &KNotificationReplyAction::activated, this, &Notification::reply);
        m_notification->setReplyAction(std::move(replyAction));
    } else {
        m_notification->setReplyAction({});
    }

    m_notification->clearActions();

    if (!otpCode.isEmpty()) {
        // Don't duplicate a "Copy <code>" action provided by the phone (see copyAuthCodeIfPresent)
        const bool phoneProvidesCopy = std::any_of(m_actions.cbegin(), m_actions.cend(), [&otpCode](const QString &action) {
            return digitsOnly(action).contains(otpCode);
        });
        if (!phoneProvidesCopy) {
            KNotificationAction *copyAction = m_notification->addAction(i18nc("@action:button %1 is a verification code", "Copy %1", otpCode));
            connect(copyAction, &KNotificationAction::activated, this, &Notification::copyOtpCode);
        }
    }

    for (const QString &actionId : std::as_const(m_actions)) {
        KNotificationAction *action = m_notification->addAction(actionId);

        connect(action, &KNotificationAction::activated, this, [this, actionId] {
            Q_EMIT actionTriggered(m_internalId, actionId);
        });
    }

    m_notification->setPixmap({});
    if (!m_hasIcon) {
        show();
        return;
    }

    const QString dir = QDir::cleanPath(m_imagesDir.absolutePath());
    const QString filePath = QDir::cleanPath(m_imagesDir.absoluteFilePath(m_payloadHash));
    if (!filePath.startsWith(dir + QStringLiteral("/"))) {
        qCWarning(KDECONNECT_PLUGIN_NOTIFICATIONS) << "Directory traversal detected";
        m_iconPath.clear();
        show();
        return;
    }

    m_iconPath = filePath;
    loadIcon(np);
}

void Notification::loadIcon(const NetworkPacket &np)
{
    m_ready = false;

    if (QFileInfo::exists(m_iconPath)) {
        applyIcon();
        show();
    } else {
        FileTransferJob *fileTransferJob = s_downloadsInProgress.value(m_iconPath);
        if (!fileTransferJob) {
            fileTransferJob = np.createPayloadTransferJob(QUrl::fromLocalFile(m_iconPath));
            fileTransferJob->start();
            s_downloadsInProgress[m_iconPath] = fileTransferJob;
        }

        connect(fileTransferJob, &FileTransferJob::result, this, [this, fileTransferJob] {
            s_downloadsInProgress.remove(m_iconPath);
            if (fileTransferJob->error()) {
                qCDebug(KDECONNECT_PLUGIN_NOTIFICATIONS) << "Error in FileTransferJob: " << fileTransferJob->errorString();
            } else {
                applyIcon();
            }
            show();
        });
    }
}

void Notification::applyIcon()
{
    if (!m_notification) {
        return;
    }

    QPixmap icon(m_iconPath, "PNG");
    m_notification->setPixmap(icon);
}

QString Notification::getConversationMessages(bool supportsMarkup) const
{
    QString conversation;
    // To avoid showing the sender of the first message twice (in title and text) at the start of the conversation.
    QString prevSender = m_title;

    for (const QJsonValue &messageElement : m_conversation) {
        const Message message = Message::fromObject(messageElement.toObject());
        if (message.sender != prevSender) {
            if (supportsMarkup) {
                conversation.append(QStringLiteral("<b>"));
                conversation.append(message.sender.toHtmlEscaped());
                conversation.append(QStringLiteral("</b>\n"));
            } else {
                conversation.append(message.sender);
                conversation.append(QStringLiteral(":\n"));
            }

            prevSender = message.sender;
        }
        conversation.append(supportsMarkup ? message.content.toHtmlEscaped() : message.content);
        conversation.append(supportsMarkup ? QStringLiteral("<br/>") : QStringLiteral("\n"));
    }
    return conversation.chopped(supportsMarkup ? 5 : 1);
}

void Notification::reply()
{
    Q_EMIT replyRequested();
}

void Notification::sendReply(const QString &message)
{
    Q_EMIT replied(message);
}

QString Notification::otpCode() const
{
    return m_config->getBool(QStringLiteral("detectOtpCodes"), true) ? m_otpCode : QString();
}

void Notification::copyOtpCode()
{
    const QString code = otpCode();
    if (code.isEmpty()) {
        return;
    }
    copyCodeToClipboard(code);

    if (m_config->getBool(QStringLiteral("markReadOnCopy"), true)) {
        markAsRead();
    }
}

void Notification::markAsRead()
{
    // Prefer the app's own "Mark as read" action, whose label is localized by the phone
    static const QRegularExpression markReadRegex(
        QStringLiteral("^(mark (as )?read|marcar como (lida|lido|leíd[ao])|als gelesen markieren|marquer comme lu|segna come letto|отметить как прочитанное)$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);

    for (const QString &action : std::as_const(m_actions)) {
        if (markReadRegex.match(action.trimmed()).hasMatch()) {
            Q_EMIT actionTriggered(m_internalId, action);
            return;
        }
    }

    // Otherwise dismiss it, so the next message is a new notification rather than a silent update
    dismiss();
}

void Notification::parseNetworkPacket(const NetworkPacket &np)
{
    m_internalId = np.get<QString>(QStringLiteral("id"));
    m_appName = np.get<QString>(QStringLiteral("appName"));
    m_ticker = np.get<QString>(QStringLiteral("ticker"));
    m_title = np.get<QString>(QStringLiteral("title"));
    m_text = np.get<QString>(QStringLiteral("text"));
    m_groupName = np.get<QString>(QStringLiteral("groupName"), QString());
    m_dismissable = np.get<bool>(QStringLiteral("isClearable"));
    m_silent = np.get<bool>(QStringLiteral("silent"));
    m_payloadHash = np.get<QString>(QStringLiteral("payloadHash"));
    m_requestReplyId = np.get<QString>(QStringLiteral("requestReplyId"), QString());
    m_conversation = np.get<QJsonArray>(QStringLiteral("conversation"), QJsonArray());

    if (isConversation()) {
        m_title = isGroupConversation() ? m_groupName : Message::fromObject(m_conversation.at(0).toObject()).sender;
        bool supportsMarkup = true; // The QML app and plasmoid, which query this via D-Bus, support HTML.
        m_text = getConversationMessages(supportsMarkup);
    }

    // Only scan the body: the title holds the sender, which for SMS may be a short code
    if (isConversation()) {
        m_otpCode = OtpDetector::findCode(Message::fromObject(m_conversation.last().toObject()).content);
    } else {
        m_otpCode = OtpDetector::findCode(m_text.isEmpty() ? m_ticker : m_text);
    }

    m_hasIcon = !m_payloadHash.isEmpty();
    m_actions.clear();

    const auto actions = np.get<QJsonArray>(QStringLiteral("actions"));
    for (const QJsonValue &value : actions) {
        m_actions.append(value.toString());
    }
}

#include "moc_notification.cpp"
