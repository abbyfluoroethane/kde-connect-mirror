/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once
#include <QObject>
#include <memory>

struct NmeaAdvertiserPrivate;

class NmeaAdvertiser : public QObject
{
    Q_OBJECT
public:
    explicit NmeaAdvertiser(QObject *parent = nullptr);
    ~NmeaAdvertiser();
    bool startAdvertising();
    void stopAdvertising();

private:
    std::unique_ptr<NmeaAdvertiserPrivate> d;
};
