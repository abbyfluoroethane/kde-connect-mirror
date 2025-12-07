/*
    SPDX-FileCopyrightText: 2025 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keylistener.h"

#include <QJsonObject>
#include <QKeyEvent>

KeyListener::KeyListener(QObject *parent)
    : QObject(parent)
{
}

KeyListener::~KeyListener() = default;

bool KeyListener::eventFilter(QObject * /*watched*/, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        QJsonObject obj;
        Q_EMIT keyReleased(obj);
    }
    return false;
}

void KeyListener::setTarget(QObject *target)
{
    if (target == m_target) {
        return;
    }

    if (m_target) {
        m_target->removeEventFilter(this);
    }

    m_target = target;

    if (m_target) {
        m_target->installEventFilter(this);
    }
}
