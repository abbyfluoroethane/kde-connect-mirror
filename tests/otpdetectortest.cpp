/**
 * SPDX-FileCopyrightText: 2026 Yuri Vidal <yurividal@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "plugins/notifications/otpdetector.h"

#include <QTest>

class OtpDetectorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFindCode_data();
    void testFindCode();
};

void OtpDetectorTest::testFindCode_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    // Should detect
    QTest::newRow("google") << QStringLiteral("G-482913 is your Google verification code.") << QStringLiteral("482913");
    QTest::newRow("plain") << QStringLiteral("Your verification code is 123456") << QStringLiteral("123456");
    QTest::newRow("otp-colon") << QStringLiteral("OTP: 9876. Do not share it with anyone.") << QStringLiteral("9876");
    QTest::newRow("spaced") << QStringLiteral("Your login code: 123 456") << QStringLiteral("123456");
    QTest::newRow("dashed") << QStringLiteral("Use 123-456 to sign in") << QStringLiteral("123456");
    QTest::newRow("invisible-separators") << QStringLiteral("Your code is 1\u20632\u20633\u20634\u20635\u20636") << QStringLiteral("123456");
    QTest::newRow("portuguese") << QStringLiteral("O seu código de verificação é 654321") << QStringLiteral("654321");
    QTest::newRow("german") << QStringLiteral("Ihr Bestätigungscode lautet 778899") << QStringLiteral("778899");
    QTest::newRow("bank-amount-and-code") << QStringLiteral("Purchase of $1250.00 at STORE. Security code 445566, valid 5 min") << QStringLiteral("445566");
    QTest::newRow("closest-to-keyword") << QStringLiteral("Order 55555 shipped. Your PIN is 4321") << QStringLiteral("4321");
    QTest::newRow("trailing-period") << QStringLiteral("Your Microsoft security code is 2468.") << QStringLiteral("2468");

    // Should not detect
    QTest::newRow("no-keyword") << QStringLiteral("Your order 123456 has shipped") << QString();
    QTest::newRow("no-digits") << QStringLiteral("Your verification code has expired") << QString();
    QTest::newRow("phone-number") << QStringLiteral("Call +1 555 123 4567 to verify your account") << QString();
    QTest::newRow("long-number") << QStringLiteral("Account code 1234567890123") << QString();
    QTest::newRow("decimal") << QStringLiteral("Login fee 1234.50") << QString();
    QTest::newRow("time") << QStringLiteral("Login at 12:30 from new device") << QString();
    QTest::newRow("empty") << QString() << QString();
}

void OtpDetectorTest::testFindCode()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    QCOMPARE(OtpDetector::findCode(text), expected);
}

QTEST_GUILESS_MAIN(OtpDetectorTest)
#include "otpdetectortest.moc"
