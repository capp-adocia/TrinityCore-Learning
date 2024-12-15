#include "ARC4.h"
#include "Errors.h"

Trinity::Crypto::ARC4::ARC4() : _ctx(EVP_CIPHER_CTX_new())
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    _cipher = EVP_CIPHER_fetch(nullptr, "RC4", nullptr);
#else
    EVP_CIPHER const* _cipher = EVP_rc4();
#endif

    EVP_CIPHER_CTX_init(_ctx);
    int result = EVP_EncryptInit_ex(_ctx, _cipher, nullptr, nullptr, nullptr);
    ASSERT(result == 1);
}

Trinity::Crypto::ARC4::~ARC4()
{
    EVP_CIPHER_CTX_free(_ctx);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_CIPHER_free(_cipher);
#endif
}

void Trinity::Crypto::ARC4::Init(uint8 const* seed, size_t len)
{
    int result1 = EVP_CIPHER_CTX_set_key_length(_ctx, len);
    ASSERT(result1 == 1);
    int result2 = EVP_EncryptInit_ex(_ctx, nullptr, nullptr, seed, nullptr);
    ASSERT(result2 == 1);
}

void Trinity::Crypto::ARC4::UpdateData(uint8* data, size_t len)
{
    int outlen = 0;
    int result1 = EVP_EncryptUpdate(_ctx, data, &outlen, data, len);
    ASSERT(result1 == 1);
    int result2 = EVP_EncryptFinal_ex(_ctx, data, &outlen);
    ASSERT(result2 == 1);
}
