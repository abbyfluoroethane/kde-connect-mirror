/**
 * SPDX-FileCopyrightText: 2026 Daniels Šatcs <dev@dshatz.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "nmeaadvertiser.h"

struct NmeaAdvertiserPrivate {
};

NmeaAdvertiser::NmeaAdvertiser(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<NmeaAdvertiserPrivate>())
{
}

NmeaAdvertiser::~NmeaAdvertiser()
{
}

void NmeaAdvertiser::startAdvertising()
{
}

void NmeaAdvertiser::stopAdvertising()
{
}

#include "moc_nmeaadvertiser.cpp"