/**
 * SPDX-FileCopyrightText: 2023 Albert Vaca <albertvaka@gmail.com>
 * SPDX-FileCopyrightText: 2023 Edward Kigwana <ekigwana@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "sslhelper.h"
#include "core_debug.h"

extern "C" {
    #include <openssl/bn.h>
    #include <openssl/err.h>
    #include <openssl/evp.h>
    #include <openssl/pem.h>
    #include <openssl/rsa.h>
    #include <openssl/x509.h>
}

namespace SslHelper
{

QString getSslError()
{
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

QSslKey generateRsaPrivateKey()
{
    // Initialize context.
    auto pctxRaw = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
    auto pctx = std::unique_ptr< EVP_PKEY_CTX, decltype(&::EVP_PKEY_CTX_free) >(pctxRaw, ::EVP_PKEY_CTX_free);
    if (!pctx) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed to allocate context " << getSslError();
        return QSslKey();
    }

    if (EVP_PKEY_keygen_init(pctx.get()) <= 0) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed to initialize context " << getSslError();
        return QSslKey();
    }

    // Set key bits.
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx.get(), 2048) <= 0) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed to set key bits " << getSslError();
        return QSslKey();
    }

    // Generate private key.
    auto pkey = std::unique_ptr< EVP_PKEY, decltype(&::EVP_PKEY_free) >(EVP_PKEY_new(), ::EVP_PKEY_free);
    if (!pkey) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed to allocate private key " << getSslError();
        return QSslKey();
    }

    auto pkey_raw = pkey.get();
    if (EVP_PKEY_keygen(pctx.get(), &pkey_raw) <= 0) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed to generate private key " << getSslError();
        return QSslKey();
    }

    // Convert private key format to PEM as required by QSslKey.
    auto bio = std::unique_ptr< BIO, decltype(&::BIO_free_all) >(BIO_new(BIO_s_mem()), ::BIO_free_all);
    if (!bio) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed to allocate I/O abstraction " << getSslError();
        return QSslKey();
    }

    if (!PEM_write_bio_PrivateKey(bio.get(), pkey_raw, nullptr, nullptr, 0, nullptr, nullptr)) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed write PEM format private key to BIO " << getSslError();
        return QSslKey();
    }

    BUF_MEM *pem = nullptr;
    if (!BIO_get_mem_ptr(bio.get(), &pem)) {
        qCWarning(KDECONNECT_CORE) << "Generate RSA Private Key failed get PEM format address " << getSslError();
        return QSslKey();
    }

    return QSslKey(QByteArray(mem->data, mem->length), QSsl::KeyAlgorithm::Rsa);
}

QSslCertificate generateSelfSignedCertificate(const QSslKey &qtPrivateKey, const QString &commonName)
{
    X509 *x509 = X509_new();
    X509_set_version(x509, 2);

    // Generate a random serial number for the certificate
    BIGNUM *serialNumber = BN_new();
    BN_rand(serialNumber, 160, -1, 0);
    ASN1_INTEGER *serial = X509_get_serialNumber(x509);
    BN_to_ASN1_INTEGER(serialNumber, serial);
    BN_free(serialNumber);

    // Set the certificate subject and issuer (self-signed)
    X509_NAME *name = X509_NAME_new();
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>(commonName.toLatin1().data()), -1, -1, 0); // Common Name
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("KDE"), -1, -1, 0); // Organization
    X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("KDE Connect"), -1, -1, 0); // Organizational Unit
    X509_set_subject_name(x509, name);
    X509_set_issuer_name(x509, name);
    X509_NAME_free(name);

    // Set the certificate validity period
    time_t now = time(nullptr);
    int a_year_in_seconds = 356 * 24 * 60 * 60;
    ASN1_TIME_set(X509_get_notBefore(x509), now - a_year_in_seconds);
    ASN1_TIME_set(X509_get_notAfter(x509), now + 10 * a_year_in_seconds);

    // Convert the QSslKey to the OpenSSL private key format and sign the certificate
    QByteArray keyPemData = qtPrivateKey.toPem();
    BIO *keyBio = BIO_new_mem_buf(keyPemData.data(), -1);
    EVP_PKEY *privateKey = PEM_read_bio_PrivateKey(keyBio, NULL, NULL, NULL);
    X509_set_pubkey(x509, privateKey);
    X509_sign(x509, privateKey, EVP_sha256());
    EVP_PKEY_free(privateKey);
    BIO_free_all(keyBio);

    // Convert to PEM which is the format needed for QSslCertificate
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(bio, x509);
    BUF_MEM *mem = nullptr;
    BIO_get_mem_ptr(bio, &mem);
    QByteArray pemData(mem->data, mem->length);
    BIO_free_all(bio);

    X509_free(x509);

    return QSslCertificate(pemData);
}

} // namespace SslHelper
