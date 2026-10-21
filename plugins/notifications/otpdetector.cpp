/**
 * SPDX-FileCopyrightText: 2026 Yuri Vidal <yurividal@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "otpdetector.h"

#include <QList>
#include <QRegularExpression>

#include <limits>

namespace
{
// Matched as case-insensitive substrings, so "code" also covers "Bestätigungscode", "passcode", etc.
const QRegularExpression &keywordRegex()
{
    static const QRegularExpression regex(
        QStringLiteral("code|otp|pin\\b|passcode|password|verif|authenticat|2fa|two[- ]factor|one[- ]time|security|login|log in|sign[- ]in|token"
                       "|c[oó]digo|codice|kod\\b|kodu|код|пароль|验证码|驗證碼|校验码|認証|確認コード|コード|인증"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return regex;
}

// 4-8 digits, or two groups of 3-4 digits separated by a space or dash, with an optional
// prefix like "G-". The lookarounds reject digits that are part of a longer number, a word,
// an amount ("12.50", "$1234") or a phone number ("+1 555 123 4567").
const QRegularExpression &candidateRegex()
{
    static const QRegularExpression regex(
        QStringLiteral("(?<![\\p{L}\\p{N}.,/:+$€£#_-])(?<!\\p{N}[ .-])(?:[A-Z]{1,3}-)?(\\d{3,4}[ -]\\d{3,4}|\\d{4,8})(?![\\p{L}\\p{N}_]|[ .,:/-]\\p{N})"),
        QRegularExpression::UseUnicodePropertiesOption);
    return regex;
}

bool looksLikeYear(const QString &digits)
{
    if (digits.size() != 4) {
        return false;
    }
    const int value = digits.toInt();
    return value >= 1950 && value <= 2099;
}
}

QString OtpDetector::findCode(const QString &input)
{
    if (input.isEmpty()) {
        return {};
    }

    // Some apps interleave invisible separators between the digits
    QString text = input;
    for (const char16_t invisible : {u'\u2063', u'\u200B', u'\u200C', u'\u200D', u'\u2060', u'\uFEFF'}) {
        text.remove(QChar(invisible));
    }

    QList<qsizetype> keywordPositions;
    auto keywordIt = keywordRegex().globalMatch(text);
    while (keywordIt.hasNext()) {
        keywordPositions.append(keywordIt.next().capturedStart());
    }
    if (keywordPositions.isEmpty()) {
        return {};
    }

    // Pick the candidate closest to a keyword, falling back to a year-like number
    QString best;
    qsizetype bestDistance = std::numeric_limits<qsizetype>::max();
    QString fallbackYear;

    auto candidateIt = candidateRegex().globalMatch(text);
    while (candidateIt.hasNext()) {
        const QRegularExpressionMatch match = candidateIt.next();
        QString digits = match.captured(1);
        digits.remove(QLatin1Char(' ')).remove(QLatin1Char('-'));

        if (looksLikeYear(digits)) {
            if (fallbackYear.isEmpty()) {
                fallbackYear = digits;
            }
            continue;
        }

        const qsizetype start = match.capturedStart(1);
        const qsizetype end = match.capturedEnd(1);
        for (const qsizetype keywordPos : std::as_const(keywordPositions)) {
            const qsizetype distance = keywordPos < start ? start - keywordPos : (keywordPos >= end ? keywordPos - end : 0);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = digits;
            }
        }
    }

    return best.isEmpty() ? fallbackYear : best;
}
