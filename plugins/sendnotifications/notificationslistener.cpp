/**
 * SPDX-FileCopyrightText: 2015 Holger Kaelberer <holger.k@elberer.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "notificationslistener.h"

#include <QStandardPaths>

#include <KConfig>
#include <KConfigGroup>

#include <core/kdeconnectplugin.h>

#include "notifyingapplication.h"
#include "plugin_sendnotification_debug.h"

NotificationsListener::NotificationsListener(KdeConnectPlugin *aPlugin)
    : QObject(aPlugin)
    , m_plugin(aPlugin)
{
    qRegisterMetaTypeStreamOperators<NotifyingApplication>("NotifyingApplication");

    setTranslatedAppName();
    loadApplications();

    connect(m_plugin->config(), &KdeConnectPluginConfig::configChanged, this, &NotificationsListener::loadApplications);
}

NotificationsListener::~NotificationsListener()
{
    qCDebug(KDECONNECT_PLUGIN_SENDNOTIFICATION) << "Destroying NotificationsListener";
}

void NotificationsListener::setTranslatedAppName()
{
    QString filePath =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("knotifications5/kdeconnect.notifyrc"), QStandardPaths::LocateFile);
    if (filePath.isEmpty()) {
        qCDebug(KDECONNECT_PLUGIN_SENDNOTIFICATION) << "Couldn't find kdeconnect.notifyrc to hide kdeconnect notifications on the devices. Using default name.";
        m_translatedAppName = QStringLiteral("KDE Connect");
        return;
    }

    KConfig config(filePath, KConfig::OpenFlag::SimpleConfig);
    KConfigGroup globalgroup(&config, QStringLiteral("Global"));
    m_translatedAppName = globalgroup.readEntry(QStringLiteral("Name"), QStringLiteral("KDE Connect"));
}

void NotificationsListener::loadApplications()
{
    m_applications.clear();
    const QVariantList list = m_plugin->config()->getList(QStringLiteral("applications"));
    for (const auto &a : list) {
        NotifyingApplication app = a.value<NotifyingApplication>();
        if (!m_applications.contains(app.name)) {
            m_applications.insert(app.name, app);
        }
    }
    // qCDebug(KDECONNECT_PLUGIN_SENDNOTIFICATION) << "Loaded" << applications.size() << " applications";
}
