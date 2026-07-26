#include "core/util/TlsTrust.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

namespace baludesk::core {
namespace {

// Aufräumhilfen, damit kein OpenSSL-Objekt bei einem frühen return liegen bleibt.
struct BioDeleter {
    void operator()(BIO* bio) const noexcept { BIO_free(bio); }
};
struct X509Deleter {
    void operator()(X509* cert) const noexcept { X509_free(cert); }
};
struct OpenSslBufferDeleter {
    void operator()(unsigned char* buffer) const noexcept { OPENSSL_free(buffer); }
};

using BioPtr = std::unique_ptr<BIO, BioDeleter>;
using X509Ptr = std::unique_ptr<X509, X509Deleter>;
using OpenSslBufferPtr = std::unique_ptr<unsigned char, OpenSslBufferDeleter>;

std::string toColonHex(const unsigned char* data, unsigned int length) {
    static constexpr char kDigits[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(length * 3);
    for (unsigned int i = 0; i < length; ++i) {
        if (i > 0) {
            result.push_back(':');
        }
        result.push_back(kDigits[(data[i] >> 4) & 0x0F]);
        result.push_back(kDigits[data[i] & 0x0F]);
    }
    return result;
}

std::string toBase64(const unsigned char* data, int length) {
    // EVP_EncodeBlock schreibt 4 Zeichen je 3 Eingabebytes plus Nullbyte.
    std::vector<unsigned char> encoded(static_cast<std::size_t>(((length + 2) / 3) * 4) + 1);
    const int written = EVP_EncodeBlock(encoded.data(), data, length);
    if (written <= 0) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(encoded.data()),
                       static_cast<std::size_t>(written));
}

}  // namespace

std::optional<CertificateIdentity> identifyCertificate(const std::string& pem) {
    if (pem.empty()) {
        return std::nullopt;
    }

    BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    if (!bio) {
        return std::nullopt;
    }

    X509Ptr certificate(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
    if (!certificate) {
        return std::nullopt;
    }

    CertificateIdentity identity;

    // Fingerabdruck: SHA-256 über das gesamte DER-kodierte Zertifikat.
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    if (X509_digest(certificate.get(), EVP_sha256(), digest.data(), &digestLength) != 1) {
        return std::nullopt;
    }
    identity.fingerprintSha256 = toColonHex(digest.data(), digestLength);

    // Pin: SHA-256 über den DER-kodierten SubjectPublicKeyInfo.
    X509_PUBKEY* publicKey = X509_get_X509_PUBKEY(certificate.get());
    if (publicKey == nullptr) {
        return std::nullopt;
    }

    unsigned char* rawSpki = nullptr;
    const int spkiLength = i2d_X509_PUBKEY(publicKey, &rawSpki);
    if (spkiLength <= 0 || rawSpki == nullptr) {
        return std::nullopt;
    }
    OpenSslBufferPtr spki(rawSpki);

    // EVP_Digest statt der freien Funktion SHA256(): letztere ist seit
    // OpenSSL 3.0 als veraltet markiert und löst unter /W4 eine Warnung aus.
    std::array<unsigned char, EVP_MAX_MD_SIZE> spkiDigest{};
    unsigned int spkiDigestLength = 0;
    if (EVP_Digest(spki.get(), static_cast<std::size_t>(spkiLength), spkiDigest.data(),
                   &spkiDigestLength, EVP_sha256(), nullptr) != 1) {
        return std::nullopt;
    }

    const std::string encoded = toBase64(spkiDigest.data(), static_cast<int>(spkiDigestLength));
    if (encoded.empty()) {
        return std::nullopt;
    }
    identity.publicKeyPin = "sha256//" + encoded;

    return identity;
}

}  // namespace baludesk::core
