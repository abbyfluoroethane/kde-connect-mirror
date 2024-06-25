/**
 * SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QPointF>

#include <core/kdeconnectplugin.h>

class InputCaptureSession;

class ShareInputDevicesPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.ping")

public:
    ShareInputDevicesPlugin(QObject *parent, const QVariantList &args);

    void receivePacket(const NetworkPacket &np) override;
    QString dbusPath() const override;

private:
    InputCaptureSession *m_inputCaptureSession;
    QPointF m_enterPosition;
    QPointF m_currentPosition;
};
