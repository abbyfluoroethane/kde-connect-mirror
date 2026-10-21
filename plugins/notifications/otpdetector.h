/**
 * SPDX-FileCopyrightText: 2026 Yuri Vidal <yurividal@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QString>

namespace OtpDetector
{
/**
 * Look for a one-time / verification code in a notification text.
 *
 * Only reports a code when the text also contains a keyword hinting that it is
 * a verification message ("code", "OTP", "PIN", ...), to avoid matching order
 * numbers, amounts or phone numbers in unrelated notifications.
 *
 * @return the code with separators stripped (e.g. "123456"), or an empty string.
 */
QString findCode(const QString &text);
}
